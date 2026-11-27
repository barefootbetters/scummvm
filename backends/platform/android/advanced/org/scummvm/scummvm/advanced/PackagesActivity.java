package org.scummvm.scummvm.advanced;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONObject;
import org.scummvm.scummvm.ScummVMActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.List;
import java.util.Locale;

/**
 * The advanced mode's screen: load a manifest, install, update and remove
 * test packages, and show diagnostics. Built in code so it needs no
 * resources; every control is a focusable button, so a D-pad can reach it.
 */
public class PackagesActivity extends Activity {
	/** Preferences key for the last manifest URL (also the release-build check's sentinel). */
	static final String PREF_MANIFEST_URL = "TVPKG_MANIFEST_URL";

	private PackageStore _store;
	private File _configFile;
	private String _engineVersion;

	private EditText _urlField;
	private TextView _status;
	private LinearLayout _packageList;
	private TextView _diagnostics;

	private PackageManifest _manifest;
	private volatile boolean _busy;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		_store = new PackageStore(this);
		_configFile = new File(getFilesDir(), "scummvm.ini");
		_engineVersion = getIntent().getStringExtra(AdvancedMode.EXTRA_ENGINE_VERSION);
		_store.log("Advanced mode opened");
		_store.cleanStaging();
		_store.cleanOrphans();
		buildUi();
		refreshDiagnostics();
	}

	// ------------------------------------------------------------------ UI

	private void buildUi() {
		ScrollView scroll = new ScrollView(this);
		LinearLayout col = new LinearLayout(this);
		col.setOrientation(LinearLayout.VERTICAL);
		int pad = dp(32);
		col.setPadding(pad, dp(24), pad, dp(24));
		scroll.addView(col);

		col.addView(text("Test packages (advanced mode, test builds only)", 22));
		col.addView(text("Manifest URL (HTTPS):", 16));

		_urlField = new EditText(this);
		_urlField.setSingleLine(true);
		_urlField.setText(prefs().getString(PREF_MANIFEST_URL, "https://"));
		col.addView(_urlField, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

		LinearLayout row = new LinearLayout(this);
		row.addView(button("Load manifest", new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				loadManifest();
			}
		}));
		row.addView(button("Back to the library", new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				finish();
			}
		}));
		row.addView(button("Share diagnostic log", new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				shareDiagnostics();
			}
		}));
		col.addView(row);

		_status = text("", 16);
		col.addView(_status);

		_packageList = new LinearLayout(this);
		_packageList.setOrientation(LinearLayout.VERTICAL);
		col.addView(_packageList);

		col.addView(text("Diagnostics", 20));
		_diagnostics = text("", 14);
		_diagnostics.setTextIsSelectable(false);
		col.addView(_diagnostics);

		setContentView(scroll);
		renderPackages();
	}

	private TextView text(String s, int sp) {
		TextView t = new TextView(this);
		t.setText(s);
		t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
		t.setPadding(0, dp(6), 0, dp(6));
		return t;
	}

	private Button button(String label, View.OnClickListener l) {
		Button b = new Button(this);
		b.setText(label);
		b.setOnClickListener(l);
		b.setFocusable(true);
		return b;
	}

	private int dp(int v) {
		return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
	}

	private SharedPreferences prefs() {
		return getSharedPreferences("tv_packages", MODE_PRIVATE);
	}

	private void setStatus(final String s) {
		runOnUiThread(new Runnable() {
			@Override
			public void run() {
				_status.setText(s);
			}
		});
	}

	private void renderPackages() {
		_packageList.removeAllViews();
		JSONObject installed = _store.readInstalled();

		if (_manifest != null) {
			for (final PackageManifest.Entry e : _manifest.entries) {
				JSONObject cur = installed.optJSONObject(e.id);
				String curVersion = cur != null ? cur.optString("version") : null;
				String state;
				if (curVersion == null) {
					state = "not installed";
				} else if (curVersion.equals(e.version)) {
					state = "installed";
				} else if (PackageManifest.compareVersions(e.version, curVersion) > 0) {
					state = "installed " + curVersion + ", update available";
				} else {
					state = "installed " + curVersion + " (manifest has the older " + e.version + ")";
				}
				_packageList.addView(text(String.format(Locale.US, "%s  [%s %s, %.1f MB]  %s",
					e.title, e.id, e.version, e.size / 1048576.0, state), 16));
				LinearLayout row = new LinearLayout(this);
				if (curVersion == null) {
					row.addView(button("Install " + e.id, new View.OnClickListener() {
						@Override
						public void onClick(View v) {
							install(e, false);
						}
					}));
				} else if (!curVersion.equals(e.version) && PackageManifest.compareVersions(e.version, curVersion) > 0) {
					row.addView(button("Update " + e.id + " to " + e.version, new View.OnClickListener() {
						@Override
						public void onClick(View v) {
							install(e, true);
						}
					}));
				}
				_packageList.addView(row);
			}
			for (String p : _manifest.problems) {
				_packageList.addView(text("Skipped " + p, 14));
			}
		}

		// Installed packages, whether or not the manifest lists them.
		for (final String id : PackageStore.keys(installed)) {
			LinearLayout row = new LinearLayout(this);
			for (final ScummVMIni.Section t : _store.targetsFor(_configFile, id)) {
				row.addView(button("Launch " + t.name, new View.OnClickListener() {
					@Override
					public void onClick(View v) {
						JSONObject o = op("launch", id);
						try {
							o.put("target", t.name);
						} catch (Exception ex) {
							throw new IllegalStateException(ex);
						}
						runOperation(o, "Launching " + t.name);
					}
				}));
			}
			row.addView(button("Remove " + id + " (keep saves)", new View.OnClickListener() {
				@Override
				public void onClick(View v) {
					runOperation(op("remove", id), "Removing " + id);
				}
			}));
			_packageList.addView(row);
		}
	}

	// ------------------------------------------------------------ actions

	private void loadManifest() {
		if (_busy) {
			return;
		}
		final String url = _urlField.getText().toString().trim();
		prefs().edit().putString(PREF_MANIFEST_URL, url).apply();
		_busy = true;
		setStatus("Loading the manifest...");
		new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					final PackageManifest m = _store.fetchManifest(url);
					runOnUiThread(new Runnable() {
						@Override
						public void run() {
							_manifest = m;
							renderPackages();
							_status.setText("Manifest: " + m.entries.size() + " package(s)");
						}
					});
				} catch (PackageStore.RefusedException e) {
					setStatus("Refused: " + e.getMessage());
				} catch (Exception e) {
					_store.log("Manifest load failed: " + e);
					setStatus("Couldn't load the manifest: " + e.getMessage());
				} finally {
					_busy = false;
				}
			}
		}, "TVPackages-manifest").start();
	}

	private void install(final PackageManifest.Entry e, final boolean update) {
		if (_busy) {
			return;
		}
		_busy = true;
		new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					_store.fetch(e, new PackageStore.Progress() {
						@Override
						public void onProgress(String message) {
							setStatus(message);
						}
					});
					JSONObject op = op(update ? "update" : "add", e.id);
					op.put("version", e.version);
					op.put("sha256", e.sha256);
					_store.setPending(op);
					setStatus("Verified and extracted. Restarting ScummVM to add it...");
					runOnUiThread(new Runnable() {
						@Override
						public void run() {
							restartScummVM();
						}
					});
				} catch (PackageStore.RefusedException ex) {
					setStatus("REFUSED: " + ex.getMessage());
				} catch (Exception ex) {
					setStatus("Failed (nothing changed): " + ex.getMessage());
				} finally {
					_busy = false;
					runOnUiThread(new Runnable() {
						@Override
						public void run() {
							refreshDiagnostics();
						}
					});
				}
			}
		}, "TVPackages-install").start();
	}

	private JSONObject op(String kind, String id) {
		try {
			JSONObject o = new JSONObject();
			o.put("op", kind);
			o.put("id", id);
			return o;
		} catch (Exception e) {
			throw new IllegalStateException(e);
		}
	}

	private void runOperation(JSONObject op, String message) {
		try {
			_store.setPending(op);
		} catch (IOException e) {
			setStatus("Failed: " + e.getMessage());
			return;
		}
		setStatus(message);
		restartScummVM();
	}

	private void restartScummVM() {
		Intent i = new Intent(this, ScummVMActivity.class);
		i.putExtra(AdvancedMode.EXTRA_RESTART, true);
		i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(i);
		finish();
	}

	// -------------------------------------------------------- diagnostics

	private String diagnosticsText() {
		StringBuilder sb = new StringBuilder();
		String appVersion = "?";
		try {
			PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
			appVersion = pi.versionName + " (" + pi.versionCode + ")";
		} catch (PackageManager.NameNotFoundException ignored) {
		}
		sb.append("Engine: ").append(_engineVersion == null ? "?" : _engineVersion).append('\n');
		sb.append("App version: ").append(appVersion).append('\n');
		sb.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
			.append(", Android ").append(Build.VERSION.RELEASE).append(" (SDK ").append(Build.VERSION.SDK_INT).append(")\n");
		JSONObject installed = _store.readInstalled();
		List<String> ids = PackageStore.keys(installed);
		sb.append("Installed packages: ").append(ids.size()).append('\n');
		for (String id : ids) {
			JSONObject rec = installed.optJSONObject(id);
			sb.append("  ").append(id).append(' ').append(rec.optString("version"))
				.append("  sha256 ").append(rec.optString("sha256"))
				.append("  in ").append(_store.rel(_store.versionDir(id, rec.optString("version")))).append('\n');
			List<ScummVMIni.Section> targets = _store.targetsFor(_configFile, id);
			if (targets.isEmpty()) {
				sb.append("    detection: no ScummVM target points into this package\n");
			}
			for (ScummVMIni.Section t : targets) {
				sb.append("    detected: target ").append(t.name)
					.append(", engine ").append(t.values.get("engineid"))
					.append(", game ").append(t.values.get("gameid"))
					.append(", \"").append(t.values.get("description")).append("\"\n");
			}
		}
		sb.append("\nStore log (latest):\n").append(_store.readLogTail(6000));
		return sb.toString();
	}

	private void refreshDiagnostics() {
		_diagnostics.setText(diagnosticsText());
		renderPackages();
	}

	private void shareDiagnostics() {
		String text = diagnosticsText();
		File out = new File(getExternalFilesDir(null), "tv-packages-diagnostics.txt");
		try {
			OutputStream os = new FileOutputStream(out);
			try {
				os.write(text.getBytes("UTF-8"));
			} finally {
				os.close();
			}
		} catch (IOException e) {
			out = null;
		}
		Intent send = new Intent(Intent.ACTION_SEND);
		send.setType("text/plain");
		send.putExtra(Intent.EXTRA_SUBJECT, "Test-package diagnostics");
		send.putExtra(Intent.EXTRA_TEXT, text);
		String saved = out != null
			? "The log is saved as Android/data/" + getPackageName() + "/files/" + out.getName()
			: "The log couldn't be saved";
		// TV builds of Android often route ACTION_SEND to a framework stub that
		// only says "You don't have an app that can do this", so look for a real handler.
		boolean hasTarget = false;
		for (ResolveInfo ri : getPackageManager().queryIntentActivities(send, 0)) {
			if (!STUB_PACKAGE.equals(ri.activityInfo.packageName)) {
				hasTarget = true;
				break;
			}
		}
		if (!hasTarget) {
			setStatus("No app on this device can share text. " + saved + " (adb pull it).");
			_store.log("No share target on this device; diagnostic log saved to the app's external files folder");
			return;
		}
		try {
			startActivity(Intent.createChooser(send, "Share the diagnostic log"));
			setStatus("Sharing the diagnostic log. " + saved + ".");
			_store.log("Diagnostic log offered to the share sheet");
		} catch (ActivityNotFoundException e) {
			setStatus("No app on this device can share text. " + saved + " (adb pull it).");
			_store.log("No share target on this device; diagnostic log saved to the app's external files folder");
		}
	}

	/** The TV framework's catch-all stub for intents no installed app handles. */
	private static final String STUB_PACKAGE = "com.android.tv.frameworkpackagestubs";
}
