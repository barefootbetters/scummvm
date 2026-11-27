package org.scummvm.scummvm;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;

import java.io.File;

/**
 * Release-build stand-in for the test-build features (see
 * backends/platform/android/advanced/). The release build compiles this file
 * instead, so it carries none of the test-package code. Every method is a
 * no-op that leaves ScummVM's behaviour unchanged.
 */
public final class TestFeatures {
	private TestFeatures() {
	}

	/** @return true when the key completes the gesture that opens the test-build tools. */
	public static boolean onKeyEvent(KeyEvent event) {
		return false;
	}

	public static void open(Activity activity, String engineVersion) {
	}

	/** @return true when the intent asks ScummVMActivity to restart the ScummVM thread. */
	public static boolean isRestartRequest(Intent intent) {
		return false;
	}

	/** Runs before each start of the ScummVM thread; may replace its arguments. */
	public static String[] beforeScummVMStart(Context context, File configFile, String[] args) {
		return args;
	}
}
