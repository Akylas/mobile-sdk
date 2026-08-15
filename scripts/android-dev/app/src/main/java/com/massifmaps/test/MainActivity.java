package com.massifmaps.test;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.WindowCompat;

import android.os.Bundle;
import android.util.Log;

import com.massifmaps.test.ui.main.MainFragment;
import com.massifmaps.test.ui.main.SecondFragment;

public class MainActivity extends AppCompatActivity {
    private final String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(TAG, "onCreate");
        // Draw edge to edge: the map runs under the status and navigation bars, and the overlay
        // (DemoPanel) insets itself so nothing lands under them.
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        setContentView(R.layout.main_activity);
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                    .replace(R.id.container, SecondFragment.newInstance())
                    .commitNow();
        }
    }

    @Override
    protected void onPostResume() {
        Log.d(TAG, "onPostResume");
        super.onPostResume();
    }

    @Override
    protected void onStart() {
        Log.d(TAG, "onStart");
        super.onStart();
    }

    @Override
    protected void onStop() {
        Log.d(TAG, "onStop");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy");
        super.onDestroy();
    }
}
