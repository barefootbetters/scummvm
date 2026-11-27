package org.scummvm.scummvm.advanced;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * A minimal line-preserving editor for scummvm.ini, used only while the
 * ScummVM thread is stopped (ScummVM reloads the file when it starts).
 * It changes only the sections and keys it's asked to.
 */
final class ScummVMIni {
	private static final Charset UTF8 = Charset.forName("UTF-8");

	private final List<String> _lines = new ArrayList<>();

	static final class Section {
		String name;
		int start; // the "[name]" line
		int end;   // exclusive
		final Map<String, String> values = new LinkedHashMap<>();
		final Map<String, Integer> lineOf = new LinkedHashMap<>();
	}

	static ScummVMIni load(File file) throws IOException {
		ScummVMIni ini = new ScummVMIni();
		if (!file.exists()) {
			return ini;
		}
		BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(file), UTF8));
		try {
			String line;
			while ((line = r.readLine()) != null) {
				ini._lines.add(line);
			}
		} finally {
			r.close();
		}
		return ini;
	}

	void save(File file) throws IOException {
		File tmp = new File(file.getPath() + ".tvpkg-tmp");
		Writer w = new OutputStreamWriter(new FileOutputStream(tmp), UTF8);
		try {
			for (String line : _lines) {
				w.write(line);
				w.write('\n');
			}
		} finally {
			w.close();
		}
		if (!tmp.renameTo(file)) {
			tmp.delete();
			throw new IOException("could not replace " + file.getName());
		}
	}

	List<Section> sections() {
		List<Section> result = new ArrayList<>();
		Section current = null;
		for (int i = 0; i < _lines.size(); ++i) {
			String t = _lines.get(i).trim();
			if (t.startsWith("[") && t.endsWith("]")) {
				if (current != null) {
					current.end = i;
				}
				current = new Section();
				current.name = t.substring(1, t.length() - 1);
				current.start = i;
				result.add(current);
			} else if (current != null && !t.startsWith("#") && t.indexOf('=') > 0) {
				int eq = t.indexOf('=');
				String key = t.substring(0, eq).trim();
				current.values.put(key, t.substring(eq + 1).trim());
				current.lineOf.put(key, i);
			}
		}
		if (current != null) {
			current.end = _lines.size();
		}
		return result;
	}

	/** Game targets (sections with a path) whose path is the given folder or inside it. */
	List<Section> targetsUnder(File dir) {
		List<Section> result = new ArrayList<>();
		String root = canonical(dir);
		for (Section s : sections()) {
			String path = s.values.get("path");
			if (path == null) {
				continue;
			}
			String p = canonical(new File(path));
			if (p.equals(root) || p.startsWith(root + File.separator)) {
				result.add(s);
			}
		}
		return result;
	}

	void setValue(String sectionName, String key, String value) {
		for (Section s : sections()) {
			if (s.name.equals(sectionName)) {
				Integer line = s.lineOf.get(key);
				if (line != null) {
					_lines.set(line, key + "=" + value);
				} else {
					_lines.add(s.start + 1, key + "=" + value);
				}
				return;
			}
		}
	}

	void removeSection(String sectionName) {
		for (Section s : sections()) {
			if (s.name.equals(sectionName)) {
				int end = s.end;
				// Keep the blank line that separates the next section.
				for (int i = end - 1; i > s.start; --i) {
					if (!_lines.get(i).trim().isEmpty()) {
						break;
					}
					end = i;
				}
				for (int i = end - 1; i >= s.start; --i) {
					_lines.remove(i);
				}
				return;
			}
		}
	}

	static String canonical(File f) {
		try {
			return f.getCanonicalPath();
		} catch (IOException e) {
			return f.getAbsolutePath();
		}
	}
}
