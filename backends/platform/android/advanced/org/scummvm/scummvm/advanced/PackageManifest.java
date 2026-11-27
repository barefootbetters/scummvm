package org.scummvm.scummvm.advanced;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * A test-package manifest. The format is documented in the TerraGlyph docs
 * (docs/reference/tv-package-manifest.md):
 *
 * <pre>
 * { "format": 1,
 *   "packages": [ { "id": "...", "title": "...", "version": "...",
 *                   "size": 123, "sha256": "...", "url": "https://..." } ] }
 * </pre>
 *
 * The app holds no host: each package's URL comes from its entry.
 */
final class PackageManifest {
	static final int FORMAT = 1;

	private static final Pattern ID = Pattern.compile("[a-z0-9][a-z0-9._-]{0,63}");
	private static final Pattern VERSION = Pattern.compile("[A-Za-z0-9][A-Za-z0-9._-]{0,31}");
	private static final Pattern SHA256 = Pattern.compile("[0-9a-f]{64}");

	static final class Entry {
		String id;
		String title;
		String version;
		long size;
		String sha256;
		String url;
	}

	final List<Entry> entries = new ArrayList<>();
	/** Entries that were skipped, with the reason. */
	final List<String> problems = new ArrayList<>();

	static PackageManifest parse(String json) throws JSONException {
		JSONObject root = new JSONObject(json);
		int format = root.optInt("format", 0);
		if (format != FORMAT) {
			throw new JSONException("unsupported manifest format " + format + " (expected " + FORMAT + ")");
		}
		JSONArray packages = root.getJSONArray("packages");
		PackageManifest manifest = new PackageManifest();
		for (int i = 0; i < packages.length(); ++i) {
			JSONObject o = packages.getJSONObject(i);
			Entry e = new Entry();
			e.id = o.optString("id", "");
			e.title = o.optString("title", e.id);
			e.version = o.optString("version", "");
			e.size = o.optLong("size", -1);
			e.sha256 = o.optString("sha256", "").toLowerCase(Locale.ROOT);
			e.url = o.optString("url", "");
			String problem = check(e);
			if (problem != null) {
				manifest.problems.add("entry " + i + " (" + e.id + "): " + problem);
			} else {
				manifest.entries.add(e);
			}
		}
		return manifest;
	}

	private static String check(Entry e) {
		if (!ID.matcher(e.id).matches()) {
			return "bad id";
		}
		if (!VERSION.matcher(e.version).matches()) {
			return "bad version";
		}
		if (e.size <= 0) {
			return "bad size";
		}
		if (!SHA256.matcher(e.sha256).matches()) {
			return "bad sha256";
		}
		if (!isHttpsUrl(e.url)) {
			return "url is not HTTPS";
		}
		return null;
	}

	static boolean isHttpsUrl(String s) {
		try {
			URL u = new URL(s);
			return "https".equals(u.getProtocol()) && u.getHost() != null && !u.getHost().isEmpty();
		} catch (MalformedURLException e) {
			return false;
		}
	}

	/** Compares dotted versions numerically where both parts are numbers. */
	static int compareVersions(String a, String b) {
		String[] pa = a.split("[._-]");
		String[] pb = b.split("[._-]");
		for (int i = 0; i < Math.max(pa.length, pb.length); ++i) {
			String x = i < pa.length ? pa[i] : "0";
			String y = i < pb.length ? pb[i] : "0";
			int c;
			try {
				c = Long.compare(Long.parseLong(x), Long.parseLong(y));
			} catch (NumberFormatException e) {
				c = x.compareTo(y);
			}
			if (c != 0) {
				return c;
			}
		}
		return 0;
	}
}
