package org.scummvm.scummvm.advanced;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyEvent;

import java.io.File;

/**
 * Entry points of the hidden advanced mode (test builds only).
 *
 * The mode opens with a D-pad sequence: up, down, up, down, left, right, left,
 * right, within ten seconds. A TV remote, a gamepad's D-pad and a keyboard's
 * arrow keys all send these keys, so no computer is needed.
 */
public final class AdvancedMode {
	static final String TAG = "TVPackages";
	static final String EXTRA_RESTART = "org.scummvm.scummvm.advanced.RESTART";
	static final String EXTRA_ENGINE_VERSION = "org.scummvm.scummvm.advanced.ENGINE_VERSION";

	// No key follows itself, so a press the remote reports twice (seen on the
	// TCL 65Q77K) still counts once: repeats of the previous key are ignored.
	private static final int[] GESTURE = {
		KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
		KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
		KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT,
		KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT
	};
	private static final long GESTURE_WINDOW_MS = 10000;

	private static int _gesturePos = 0;
	private static long _gestureStart = 0;
	private static int _lastKey = 0;

	private AdvancedMode() {
	}

	public static boolean onKeyEvent(KeyEvent event) {
		if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() != 0) {
			return false;
		}
		int code = event.getKeyCode();
		// Only the four directions take part. A TV remote's ring often also
		// registers its centre (OK) when pressed near the edge; ignore that and
		// every other key rather than restarting the match.
		if (code != KeyEvent.KEYCODE_DPAD_UP && code != KeyEvent.KEYCODE_DPAD_DOWN
				&& code != KeyEvent.KEYCODE_DPAD_LEFT && code != KeyEvent.KEYCODE_DPAD_RIGHT) {
			return false;
		}
		long now = SystemClock.uptimeMillis();
		if (_gesturePos > 0 && now - _gestureStart > GESTURE_WINDOW_MS) {
			_gesturePos = 0;
			_lastKey = 0;
		}
		if (code == _lastKey) {
			return false;
		}
		_lastKey = code;
		if (code == GESTURE[_gesturePos]) {
			if (_gesturePos == 0) {
				_gestureStart = now;
			}
			_gesturePos++;
			if (_gesturePos == GESTURE.length) {
				_gesturePos = 0;
				Log.i(TAG, "Advanced mode gesture recognized");
				return true;
			}
		} else {
			// Restart the match; this key may itself begin a new sequence.
			_gesturePos = (code == GESTURE[0]) ? 1 : 0;
			_gestureStart = now;
		}
		return false;
	}

	public static void open(Activity activity, String engineVersion) {
		Intent intent = new Intent(activity, PackagesActivity.class);
		intent.putExtra(EXTRA_ENGINE_VERSION, engineVersion);
		activity.startActivity(intent);
	}

	public static boolean isRestartRequest(Intent intent) {
		return intent != null && intent.getBooleanExtra(EXTRA_RESTART, false);
	}

	public static String[] beforeScummVMStart(Context context, File configFile, String[] args) {
		try {
			return new PackageStore(context).applyPending(configFile, args);
		} catch (Exception e) {
			Log.e(TAG, "Applying the pending package operation failed", e);
			return args;
		}
	}
}
