package com.massifmaps.test.ui.main;

import androidx.lifecycle.ViewModelProviders;

import android.animation.Animator;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import com.massifmaps.test.R;
import com.massifmaps.ui.MapView;

public class MainFragment extends Fragment {
    private final String TAG = "MainFragment";
    private MainViewModel mViewModel;

    public static MainFragment newInstance() {
        return new MainFragment();
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.main_fragment, container, false);
        Button button = (Button) view.findViewById(R.id.button);
        button.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                openSecondFragment();
            }
        });
        return view;
    }
    @Override
    public void onDestroyView() {
        super.onDestroyView();
    }
    @Override
    public void onDestroy() {
        super.onDestroy();
    }
    @Override
    public void onPause() {
        super.onPause();
    }
    @Override
    public void onStop() {
        super.onStop();
    }
    @Override
    public Animator onCreateAnimator(int transit, boolean enter, int nextAnim) {
        Animator result = super.onCreateAnimator( transit,  enter,  nextAnim);
        return  result;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mViewModel = ViewModelProviders.of(this).get(MainViewModel.class);
        // TODO: Use the ViewModel
    }


    public void openSecondFragment() {
        SecondFragment nextFrag= new SecondFragment();
        getActivity().getSupportFragmentManager().beginTransaction()
                .replace(R.id.container, nextFrag, "findThisFragment")
                .addToBackStack(null)
                .commit();
    }


}
