package org.scummvm.scummvm.advanced;

import android.content.Context;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipInputStream;

/**
 * Downloads, verifies, extracts and tracks test packages.
 *
 * Layout under getFilesDir() (kept across app updates, removed on uninstall):
 * <pre>
 *   packages/.staging/        downloads and extraction in progress
 *   packages/&lt;id&gt;/&lt;version&gt;/  an installed package (game data only)
 *   packages/installed.json   id -&gt; {version, sha256, installed}
 *   packages/pending.json     an operation for ScummVM's config, applied while the ScummVM thread is stopped
 *   packages/store.log        this store's log
 * </pre>
 *
 * A package is data only: nothing in it is ever loaded or run by the app.
 */
final class PackageStore {
	/** File name endings refused in a package. */
	private static final String[] REFUSED_ENDINGS = { ".so", ".dex", ".jar", ".apk", ".odex", ".oat", ".vdex" };
	private static final long MAX_MANIFEST_BYTES = 1024 * 1024;
	private static final long MAX_EXTRACTED_BYTES = 8L * 1024 * 1024 * 1024;

	static final class RefusedException extends Exception {
		RefusedException(String reason) {
			super(reason);
		}
	}

	interface Progress {
		void onProgress(String message);
	}

	final Context _context;
	final File _filesDir;
	final File _root;
	final File _staging;

	PackageStore(Context context) {
		_context = context;
		_filesDir = context.getFilesDir();
		_root = new File(_filesDir, "packages");
		_staging = new File(_root, ".staging");
	}

	File versionDir(String id, String version) {
		return new File(new File(_root, id), version);
	}

	/** A path for logs and diagnostics, relative to the app's files folder. */
	String rel(File f) {
		String base = ScummVMIni.canonical(_filesDir);
		String p = ScummVMIni.canonical(f);
		return p.startsWith(base + File.separator) ? p.substring(base.length() + 1) : p;
	}

	// ---------------------------------------------------------------- log

	synchronized void log(String message) {
		Log.i(AdvancedMode.TAG, message);
		try {
			_root.mkdirs();
			OutputStream out = new FileOutputStream(new File(_root, "store.log"), true);
			try {
				String stamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
				out.write((stamp + "  " + message + "\n").getBytes("UTF-8"));
			} finally {
				out.close();
			}
		} catch (IOException ignored) {
		}
	}

	String readLogTail(int maxBytes) {
		File f = new File(_root, "store.log");
		if (!f.exists()) {
			return "";
		}
		try {
			byte[] all = readFully(new FileInputStream(f), Long.MAX_VALUE);
			int from = Math.max(0, all.length - maxBytes);
			return new String(all, from, all.length - from, "UTF-8");
		} catch (IOException e) {
			return "(log unreadable: " + e.getMessage() + ")";
		}
	}

	// ---------------------------------------------------------- installed

	JSONObject readInstalled() {
		return readJson(new File(_root, "installed.json"));
	}

	void writeInstalled(JSONObject installed) throws IOException {
		writeJson(new File(_root, "installed.json"), installed);
	}

	// ------------------------------------------------------------ network

	/** Fetches a manifest over HTTPS. */
	PackageManifest fetchManifest(String url) throws IOException, JSONException, RefusedException {
		if (!PackageManifest.isHttpsUrl(url)) {
			throw new RefusedException("the manifest URL must be HTTPS");
		}
		HttpURLConnection c = open(url);
		try {
			byte[] body = readFully(c.getInputStream(), MAX_MANIFEST_BYTES);
			PackageManifest m = PackageManifest.parse(new String(body, "UTF-8"));
			log("Manifest loaded from " + new URL(url).getHost() + ": " + m.entries.size() + " package(s)"
				+ (m.problems.isEmpty() ? "" : ", skipped " + m.problems));
			return m;
		} finally {
			c.disconnect();
		}
	}

	private HttpURLConnection open(String url) throws IOException, RefusedException {
		HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
		c.setConnectTimeout(15000);
		c.setReadTimeout(30000);
		// HttpURLConnection follows redirects only within one protocol, so an
		// HTTPS request can't be redirected to plain HTTP.
		c.setInstanceFollowRedirects(true);
		int code = c.getResponseCode();
		if (!"https".equals(c.getURL().getProtocol())) {
			c.disconnect();
			throw new RefusedException("redirected away from HTTPS");
		}
		if (code != HttpURLConnection.HTTP_OK) {
			c.disconnect();
			throw new IOException("HTTP " + code + " from " + c.getURL().getHost());
		}
		return c;
	}

	// ------------------------------------------------------------ install

	/**
	 * Downloads a package to the staging folder, refuses it unless its size and
	 * SHA-256 match the manifest, extracts it (data only), and moves it to
	 * packages/&lt;id&gt;/&lt;version&gt;/. Doesn't touch an installed version or
	 * ScummVM's configuration.
	 */
	File fetch(PackageManifest.Entry e, Progress progress) throws IOException, RefusedException {
		JSONObject installed = readInstalled();
		JSONObject current = installed.optJSONObject(e.id);
		if (current != null && e.version.equals(current.optString("version"))) {
			throw new RefusedException(e.id + " " + e.version + " is already installed");
		}
		cleanStaging();
		_staging.mkdirs();
		File zip = new File(_staging, e.id + "-" + e.version + ".zip");
		File tmpDir = new File(_staging, e.id + "-" + e.version + ".d");
		try {
			log("Downloading " + e.id + " " + e.version + " from " + new URL(e.url).getHost());
			String sha = download(e, zip, progress);
			if (zip.length() != e.size) {
				throw new RefusedException("size " + zip.length() + " doesn't match the manifest's " + e.size);
			}
			if (!sha.equals(e.sha256)) {
				throw new RefusedException("SHA-256 " + sha + " doesn't match the manifest's " + e.sha256);
			}
			log("Verified " + e.id + " " + e.version + ": SHA-256 " + sha + ", " + e.size + " bytes");
			progress.onProgress("Verified. Extracting...");
			int files = extract(zip, tmpDir);
			File dest = versionDir(e.id, e.version);
			if (dest.exists()) {
				deleteRecursive(dest);
			}
			dest.getParentFile().mkdirs();
			if (!tmpDir.renameTo(dest)) {
				throw new IOException("could not move the package into " + rel(dest));
			}
			log("Extracted " + files + " file(s) to " + rel(dest));
			return dest;
		} catch (RefusedException ex) {
			log("REFUSED " + e.id + " " + e.version + ": " + ex.getMessage());
			throw ex;
		} catch (IOException ex) {
			log("FAILED " + e.id + " " + e.version + ": " + ex);
			throw ex;
		} finally {
			zip.delete();
			deleteRecursive(tmpDir);
		}
	}

	private String download(PackageManifest.Entry e, File zip, Progress progress) throws IOException, RefusedException {
		MessageDigest md = sha256();
		HttpURLConnection c = open(e.url);
		try {
			InputStream in = c.getInputStream();
			OutputStream out = new FileOutputStream(zip);
			try {
				byte[] buf = new byte[64 * 1024];
				long total = 0;
				long nextReport = 0;
				int n;
				while ((n = in.read(buf)) > 0) {
					total += n;
					if (total > e.size) {
						throw new RefusedException("larger than the manifest's size " + e.size);
					}
					md.update(buf, 0, n);
					out.write(buf, 0, n);
					if (total >= nextReport) {
						progress.onProgress(String.format(Locale.US, "Downloading %s: %.1f of %.1f MB",
							e.id, total / 1048576.0, e.size / 1048576.0));
						nextReport = total + 1024 * 1024;
					}
				}
			} finally {
				out.close();
				in.close();
			}
		} finally {
			c.disconnect();
		}
		return hex(md.digest());
	}

	/**
	 * Extracts a data-only zip; refuses executables and paths outside the folder.
	 * Static and free of Android classes, so a desktop JVM can exercise it too.
	 */
	static int extract(File zip, File dir) throws IOException, RefusedException {
		deleteRecursive(dir);
		dir.mkdirs();
		String root = dir.getCanonicalPath() + File.separator;
		ZipInputStream zin = new ZipInputStream(new FileInputStream(zip));
		int files = 0;
		long total = 0;
		try {
			ZipEntry ze;
			byte[] buf = new byte[64 * 1024];
			while ((ze = zin.getNextEntry()) != null) {
				String name = ze.getName();
				File out = new File(dir, name);
				String path = out.getCanonicalPath();
				if (!(path + (ze.isDirectory() ? File.separator : "")).startsWith(root) || path.equals(root)) {
					throw new RefusedException("entry \"" + name + "\" points outside the package folder");
				}
				if (ze.isDirectory()) {
					out.mkdirs();
					continue;
				}
				String lower = name.toLowerCase(Locale.ROOT);
				for (String ending : REFUSED_ENDINGS) {
					if (lower.endsWith(ending)) {
						throw new RefusedException("entry \"" + name + "\" is executable code (" + ending + ")");
					}
				}
				out.getParentFile().mkdirs();
				OutputStream os = new FileOutputStream(out);
				try {
					byte[] head = new byte[4];
					int headLen = 0;
					int n;
					while (headLen < 4 && (n = zin.read(head, headLen, 4 - headLen)) > 0) {
						headLen += n;
					}
					if (headLen == 4) {
						if (head[0] == 0x7F && head[1] == 'E' && head[2] == 'L' && head[3] == 'F') {
							throw new RefusedException("entry \"" + name + "\" is an ELF executable");
						}
						if (head[0] == 'd' && head[1] == 'e' && head[2] == 'x' && head[3] == '\n') {
							throw new RefusedException("entry \"" + name + "\" is Dalvik bytecode");
						}
					}
					os.write(head, 0, headLen);
					total += headLen;
					while ((n = zin.read(buf)) > 0) {
						total += n;
						if (total > MAX_EXTRACTED_BYTES) {
							throw new RefusedException("extracts to more than " + MAX_EXTRACTED_BYTES + " bytes");
						}
						os.write(buf, 0, n);
					}
				} finally {
					os.close();
				}
				files++;
			}
		} catch (ZipException e) {
			// Android 14+ rejects some entries (such as "../" paths) itself, before the checks above run.
			throw new RefusedException("zip entry rejected: " + e.getMessage());
		} finally {
			zin.close();
		}
		if (files == 0) {
			throw new RefusedException("the package has no files");
		}
		return files;
	}

	void cleanStaging() {
		deleteRecursive(_staging);
	}

	/** Removes version folders that installed.json doesn't name (left by an interrupted update). */
	void cleanOrphans() {
		JSONObject installed = readInstalled();
		JSONObject pending = readJson(new File(_root, "pending.json"));
		File[] ids = _root.listFiles();
		if (ids == null) {
			return;
		}
		for (File idDir : ids) {
			if (!idDir.isDirectory() || idDir.getName().startsWith(".")) {
				continue;
			}
			JSONObject cur = installed.optJSONObject(idDir.getName());
			File[] versions = idDir.listFiles();
			if (versions != null) {
				for (File v : versions) {
					boolean keep = cur != null && v.getName().equals(cur.optString("version"));
					keep |= idDir.getName().equals(pending.optString("id")) && v.getName().equals(pending.optString("version"));
					if (!keep) {
						log("Cleaning up " + rel(v) + " (not an installed version)");
						deleteRecursive(v);
					}
				}
			}
			String[] left = idDir.list();
			if (left == null || left.length == 0) {
				idDir.delete();
			}
		}
	}

	// ---------------------------------------------- operations on ScummVM

	/** Records an operation to apply while the ScummVM thread is stopped. */
	void setPending(JSONObject op) throws IOException {
		writeJson(new File(_root, "pending.json"), op);
	}

	/**
	 * Applies the pending operation, if any, to ScummVM's configuration file.
	 * Called with the ScummVM thread stopped.
	 *
	 * @return the ScummVM arguments to start with
	 */
	String[] applyPending(File configFile, String[] args) throws IOException, JSONException {
		File pendingFile = new File(_root, "pending.json");
		if (!pendingFile.exists()) {
			return args;
		}
		JSONObject op = readJson(pendingFile);
		String kind = op.optString("op");
		String id = op.optString("id");
		String[] result = args;
		JSONObject installed = readInstalled();

		if ("add".equals(kind) || "update".equals(kind)) {
			String version = op.getString("version");
			File newDir = versionDir(id, version);
			if (!newDir.isDirectory()) {
				log("Pending " + kind + " of " + id + " " + version + " dropped: " + rel(newDir) + " is missing");
				pendingFile.delete();
				return args;
			}
			boolean needAdd = true;
			JSONObject old = installed.optJSONObject(id);
			if ("update".equals(kind) && old != null) {
				File oldDir = versionDir(id, old.getString("version"));
				ScummVMIni ini = ScummVMIni.load(configFile);
				List<ScummVMIni.Section> targets = ini.targetsUnder(oldDir);
				String oldRoot = ScummVMIni.canonical(oldDir);
				needAdd = targets.isEmpty();
				for (ScummVMIni.Section s : targets) {
					String path = ScummVMIni.canonical(new File(s.values.get("path")));
					File moved = new File(newDir, path.substring(oldRoot.length()));
					if (moved.isDirectory()) {
						ini.setValue(s.name, "path", moved.getPath());
						log("Switched target " + s.name + " to " + rel(moved));
					} else {
						ini.removeSection(s.name);
						needAdd = true;
						log("Target " + s.name + ": " + rel(moved) + " doesn't exist in the new version; re-adding");
					}
				}
				ini.save(configFile);
			}
			JSONObject rec = new JSONObject();
			rec.put("version", version);
			rec.put("sha256", op.optString("sha256"));
			rec.put("installed", new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ", Locale.US).format(new Date()));
			installed.put(id, rec);
			writeInstalled(installed);
			if (old != null && !old.getString("version").equals(version)) {
				deleteRecursive(versionDir(id, old.getString("version")));
				log("Removed the old version " + id + " " + old.getString("version"));
			}
			if (needAdd) {
				result = new String[] { "ScummVM", "--add", "--path=" + newDir.getPath(), "--recursive", "--no-exit" };
				log("Adding " + rel(newDir) + " through ScummVM's --add --recursive");
			}
			log("Installed " + id + " " + version);
		} else if ("remove".equals(kind)) {
			File idDir = new File(_root, id);
			ScummVMIni ini = ScummVMIni.load(configFile);
			for (ScummVMIni.Section s : ini.targetsUnder(idDir)) {
				ini.removeSection(s.name);
				log("Removed target " + s.name + " from the library (its saves are kept)");
			}
			ini.save(configFile);
			deleteRecursive(idDir);
			installed.remove(id);
			writeInstalled(installed);
			log("Removed " + id);
		} else if ("launch".equals(kind)) {
			result = new String[] { "ScummVM", op.getString("target") };
			log("Launching target " + op.getString("target"));
		}
		pendingFile.delete();
		return result;
	}

	/** ScummVM targets (from scummvm.ini) that point into a package. */
	List<ScummVMIni.Section> targetsFor(File configFile, String id) {
		try {
			return ScummVMIni.load(configFile).targetsUnder(new File(_root, id));
		} catch (IOException e) {
			return new ArrayList<>();
		}
	}

	// ------------------------------------------------------------ helpers

	static MessageDigest sha256() {
		try {
			return MessageDigest.getInstance("SHA-256");
		} catch (NoSuchAlgorithmException e) {
			throw new IllegalStateException(e);
		}
	}

	static String hex(byte[] bytes) {
		StringBuilder sb = new StringBuilder();
		for (byte b : bytes) {
			sb.append(String.format(Locale.US, "%02x", b & 0xff));
		}
		return sb.toString();
	}

	static byte[] readFully(InputStream in, long max) throws IOException {
		try {
			ByteArrayOutputStream bos = new ByteArrayOutputStream();
			byte[] buf = new byte[16 * 1024];
			int n;
			while ((n = in.read(buf)) > 0) {
				if (bos.size() + n > max) {
					throw new IOException("response larger than " + max + " bytes");
				}
				bos.write(buf, 0, n);
			}
			return bos.toByteArray();
		} finally {
			in.close();
		}
	}

	static JSONObject readJson(File f) {
		if (!f.exists()) {
			return new JSONObject();
		}
		try {
			return new JSONObject(new String(readFully(new FileInputStream(f), MAX_MANIFEST_BYTES), "UTF-8"));
		} catch (IOException | JSONException e) {
			Log.w(AdvancedMode.TAG, "Unreadable " + f.getName() + ": " + e);
			return new JSONObject();
		}
	}

	static void writeJson(File f, JSONObject o) throws IOException {
		f.getParentFile().mkdirs();
		File tmp = new File(f.getPath() + ".tmp");
		OutputStream out = new FileOutputStream(tmp);
		try {
			try {
				out.write(o.toString(2).getBytes("UTF-8"));
			} catch (JSONException e) {
				throw new IOException(e);
			}
		} finally {
			out.close();
		}
		if (!tmp.renameTo(f)) {
			tmp.delete();
			throw new IOException("could not write " + f.getName());
		}
	}

	static void deleteRecursive(File f) {
		if (f == null || !f.exists()) {
			return;
		}
		File[] children = f.listFiles();
		if (children != null) {
			for (File c : children) {
				deleteRecursive(c);
			}
		}
		f.delete();
	}

	static List<String> keys(JSONObject o) {
		List<String> result = new ArrayList<>();
		Iterator<String> it = o.keys();
		while (it.hasNext()) {
			result.add(it.next());
		}
		return result;
	}
}
