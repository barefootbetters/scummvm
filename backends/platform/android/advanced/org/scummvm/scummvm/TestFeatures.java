package org.scummvm.scummvm;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;

import org.scummvm.scummvm.advanced.AdvancedMode;

import java.io.File;

/**
 * Test-build features: the hidden advanced mode for authorized test packages.
 * Compiled into the debug build only; the release build uses the stub in
 * backends/platform/android/advanced-stub/, which has the same methods.
 */
public final class TestFeatures {
	private TestFeatures() {
	}

	/** @return true when the key completes the gesture that opens the test-build tools. */
	public static boolean onKeyEvent(KeyEvent event) {
		return AdvancedMode.onKeyEvent(event);
	}

	public static void open(Activity activity, String engineVersion) {
		AdvancedMode.open(activity, engineVersion);
	}

	/** @return true when the intent asks ScummVMActivity to restart the ScummVM thread. */
	public static boolean isRestartRequest(Intent intent) {
		return AdvancedMode.isRestartRequest(intent);
	}

	/** Runs before each start of the ScummVM thread; may replace its arguments. */
	public static String[] beforeScummVMStart(Context context, File configFile, String[] args) {
		return AdvancedMode.beforeScummVMStart(context, configFile, args);
	}
}
