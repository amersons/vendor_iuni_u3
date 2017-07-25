/******************************************************************************
 * @file    DigitalPenSettingsActivity.java
 * @brief   Digital Pen Settings
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *  ---------------------------------------------------------------------------
 ******************************************************************************/
package qti.digitalpen;

import com.qti.snapdragon.digitalpen.IDigitalPenService;
import com.qti.snapdragon.digitalpen.IDigitalPenEventCallback;
import com.qti.snapdragon.digitalpen.util.DigitalPenConfig;
import com.qti.snapdragon.digitalpen.util.DigitalPenEvent;

import android.app.Activity;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.CompoundButton;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;
import android.widget.Toast;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.util.Log;
import android.os.SystemProperties;
import android.os.RemoteException;
import android.os.Bundle;
import android.os.Handler;
import android.os.ServiceManager;

public class DigitalPenSettingsActivity extends Activity implements OnCheckedChangeListener {
    private static final String TAG = "DigitalPenSettingsActivity";

    private boolean isDigitalPenEnabled;
    // TODO: wrap IDigitalPenService, e.g. with DigitalPenGlobalSettings
    private IDigitalPenService serviceHandle;
    private final IDigitalPenEventCallback eventCb = new IDigitalPenEventCallback.Stub() {
        @Override
        public void onDigitalPenPropEvent(DigitalPenEvent event) {
            // Listen for power state change
            int powerState = event
                    .getParameterValue(DigitalPenEvent.PARAM_CURRENT_POWER_STATE);
            Log.d(TAG, "Power state: " + powerState);
            if (needToHandlePowerStateEvent(powerState)) {
                final boolean newEnableValue = powerState == DigitalPenEvent.POWER_STATE_ACTIVE;
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        isDigitalPenEnabled = newEnableValue;
                        updateUi();
                    }
                });

            }

        }

        protected boolean needToHandlePowerStateEvent(int powerState) {
            // other power states, such as idle, are informational
            return powerState == DigitalPenEvent.POWER_STATE_ACTIVE
                    || powerState == DigitalPenEvent.POWER_STATE_OFF;
        }
    };
    private final Handler mHandler = new Handler();

    private EditText inRangeDistance;

    private DigitalPenConfig config;

    private EditText onScreenHoverMaxDistance;

    private EditText offScreenHoverMaxDistance;

    private EditText eraseButtonIndex;

    private CompoundButton eraseButtonEnabled;

    private boolean initializingWidgetValues;

    public void updateUi() {
        Log.d(TAG, "updateUi");
        // set enabled/disabled
        disableEnableControls(isDigitalPenEnabled, (ViewGroup) findViewById(R.id.topLayout));
        setCheckbox(R.id.toggleButtonEnable, isDigitalPenEnabled);
        findViewById(R.id.toggleButtonEnable).setEnabled(true);
    }

    private void disableEnableControls(boolean enable, ViewGroup vg) {
        for (int i = 0; i < vg.getChildCount(); i++) {
            View child = vg.getChildAt(i);
            child.setEnabled(enable);
            if (child instanceof ViewGroup) {
                disableEnableControls(enable, (ViewGroup) child);
            }
        }
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        // initialize edit text callbacks
        inRangeDistance = (EditText) findViewById(R.id.editTextInRangeDistance);
        setEditDoneListeners(inRangeDistance);
        onScreenHoverMaxDistance = (EditText) findViewById(R.id.editTextOnScreenHoverMaxDistance);
        setEditDoneListeners(onScreenHoverMaxDistance);
        offScreenHoverMaxDistance = (EditText) findViewById(R.id.editTextOffScreenHoverMaxDistance);
        setEditDoneListeners(offScreenHoverMaxDistance);
        eraseButtonIndex = (EditText) findViewById(R.id.editTextEraserIndex);
        setEditDoneListeners(eraseButtonIndex);
        eraseButtonEnabled = (CompoundButton) findViewById(R.id.switchEraserEnable);

        // initialize switch callbacks; onClick doesn't capture finger slide
        final int[] switchIds = {
                R.id.switchSmarterStand,
                R.id.switchOnScreenHover,
                R.id.switchOffScreenHover,
                R.id.switchOnScreenHoverIcon,
                R.id.switchOffScreenHoverIcon,
                R.id.switchEraserEnable,
                R.id.switchStopPenOnScreenOff,
                R.id.switchStartPenOnBoot
        };
        for (int switchId : switchIds) {
            ((CompoundButton) findViewById(switchId)).setOnCheckedChangeListener(this);
        }

        // Start the service with no callbacks
        final String serviceName = IDigitalPenService.class.getName();
        Log.e(TAG, "serviceName: " + serviceName);

        serviceHandle = IDigitalPenService.Stub
                .asInterface(ServiceManager.getService("DigitalPen"));
        if (null == serviceHandle) {
            Toast.makeText(this, "Service unavailable", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        Log.d(TAG, "Connected to the service");
        try {
            // Register the event callback(s) and update config with what's
            // shown on UI
            serviceHandle.registerEventCallback(eventCb);
        } catch (RemoteException e) {
            Log.e(TAG, "Remote service error: " + e.getMessage());
        }

        isDigitalPenEnabled = !(SystemProperties.get("init.svc.usf_epos",
                "stopped")).equals("stopped");
    }

    @Override
    protected void onResume() {
        Log.d(TAG, "onResume");
        super.onResume();

        try {
            config = serviceHandle.getConfig();
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception trying to get config");
            e.printStackTrace();
        }

        if (null == config) {
            Toast.makeText(this, "Failed to load config", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        initializeWidgetValues();
    }

    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        initializingWidgetValues = true;
        super.onRestoreInstanceState(savedInstanceState);
        initializingWidgetValues = false;
    };

    private void initializeWidgetValues() {
        initializingWidgetValues = true;
        isDigitalPenEnabled = false;
        if (serviceHandle != null) {
            try {
                isDigitalPenEnabled = serviceHandle.isEnabled();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
        setCheckbox(R.id.toggleButtonEnable, isDigitalPenEnabled);

        // set values read from config object
        if (config.getOffScreenMode() == DigitalPenConfig.DP_OFF_SCREEN_MODE_DISABLED) {
            setCheckbox(R.id.radioOffScreenDisabled, true);
        } else {
            setCheckbox(R.id.radioOffScreenDuplicate, true);
        }
        setCheckbox(R.id.switchSmarterStand, config.isSmarterStandEnabled());
        inRangeDistance.setText(Integer.toString(config.getTouchRange()));

        setCheckbox(R.id.switchStopPenOnScreenOff, config.isStopPenOnScreenOffEnabled());

        setCheckbox(R.id.switchStartPenOnBoot, config.isStartPenOnBootEnabled());

        setCheckbox(R.id.switchOnScreenHover, config.isOnScreenHoverEnabled());
        onScreenHoverMaxDistance.setText(Integer.toString(config.getOnScreenHoverMaxRange()));
        setCheckbox(R.id.switchOnScreenHoverIcon, config.isShowingOnScreenHoverIcon());

        setCheckbox(R.id.switchOffScreenHover, config.isOffScreenHoverEnabled());
        offScreenHoverMaxDistance.setText(Integer.toString(config.getOffScreenHoverMaxRange()));
        setCheckbox(R.id.switchOffScreenHoverIcon, config.isShowingOffScreenHoverIcon());

        if (config.getOffSceenPortraitSide() == DigitalPenConfig.DP_PORTRAIT_SIDE_LEFT) {
            setCheckbox(R.id.radioOffScreenLocationLeft, true);
        } else {
            setCheckbox(R.id.radioOffScreenLocationRight, true);
        }

        int buttonIndex = config.getEraseButtonIndex();
        setCheckbox(R.id.switchEraserEnable, buttonIndex != -1);
        eraseButtonIndex.setText(buttonIndex == -1 ? "0" : Integer
                .toString(buttonIndex));
        if (config.getEraseButtonMode() == DigitalPenConfig.DP_ERASE_MODE_HOLD) {
            setCheckbox(R.id.radioEraserBehaviorHold, true);
        } else {
            setCheckbox(R.id.radioEraserBehaviorToggle, true);
        }

        if (config.getPowerSave() == DigitalPenConfig.DP_POWER_PROFILE_OPTIMIZE_ACCURACY) {
            setCheckbox(R.id.radioPowerModeAccuracy, true);
        } else {
            setCheckbox(R.id.radioPowerModePower, true);
        }
        updateUi();
        initializingWidgetValues = false;
    }

    private void setCheckbox(int id, boolean set) {
        ((CompoundButton) findViewById(id)).setChecked(set);
    }

    private void setEditDoneListeners(EditText editText) {
        editText.setOnEditorActionListener(new OnEditorActionListener() {

            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    return integerTextFieldDone(v);
                }
                return false;
            }
        });
        editText.setOnFocusChangeListener(new OnFocusChangeListener() {

            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (!hasFocus) {
                    integerTextFieldDone((TextView) v);
                }
            }
        });
    }

    private boolean integerTextFieldDone(TextView v) {
        try {
            final int value = Integer.parseInt(v.getText().toString());
            switch (v.getId()) {
                case R.id.editTextInRangeDistance:
                    config.setTouchRange(value);
                    break;
                case R.id.editTextOnScreenHoverMaxDistance:
                    config.setOnScreenHoverMaxRange(value);
                    break;
                case R.id.editTextOffScreenHoverMaxDistance:
                    config.setOffScreenHoverMaxRange(value);
                    break;
                case R.id.editTextEraserIndex:
                    if (eraseButtonEnabled.isChecked()) {
                        config.setEraseButtonIndex(value);
                    } else {
                        config.setEraseButtonIndex(-1);
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown text view in editTextDone: " + v);
            }
            setGlobalConfig();
        } catch (NumberFormatException x) {
            x.printStackTrace();
            Toast.makeText(this, "Illegal value '" + v.getText() + "', expected numeric string",
                    Toast.LENGTH_LONG).show();
        }
        return true;
    }

    public void onClickEnableButton(View v) {
        Log.d(TAG, "onClickEnableButton: " + ((CompoundButton) v).isChecked());
        if (null == serviceHandle) { // No connection yet
            return; // Nothing to do
        }
        if (isDigitalPenEnabled) {
            try {
                if (!serviceHandle.disable()) {
                    Log.e(TAG,
                            "Failed to disable daemon");
                    Toast.makeText(this,
                            "Failed to disable Digital Pen daemon",
                            Toast.LENGTH_SHORT).show();
                    // Return button to its previous state
                    updateUi();
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Remote service error: " + e.getMessage());
            }
        } else {
            try {
                if (!serviceHandle.enable()) {
                    Log.e(TAG,
                            "Failed to enable daemon");
                    Toast.makeText(this,
                            "Failed to enable Digital Pen daemon",
                            Toast.LENGTH_SHORT).show();
                    // Return button to its previous state
                    updateUi();
                }
            } catch (RemoteException e) {
                Log.e(TAG, "Remote service error: " + e.getMessage());
            }
        }
    }

    private void setGlobalConfig() {
        Log.d(TAG, "setGlobalConfig");
        boolean success = false;
        try {
            success = serviceHandle.setGlobalConfig(config);
        } catch (RemoteException e) {
            Log.e(TAG, "Remote exception trying to set global config");
            e.printStackTrace();
        } finally {
            if (!success) {
                Log.e(TAG, "setGlobalConfig failed");
                Toast.makeText(this, "Failed to load config", Toast.LENGTH_SHORT).show();
            } else {
                Log.d(TAG, "setGlobalConfig successful");
            }
        }
    }

    public void onClickRadioOffScreenMode(View v) {
        switch (v.getId()) {
            case R.id.radioOffScreenDisabled:
                config.setOffScreenMode(DigitalPenConfig.DP_OFF_SCREEN_MODE_DISABLED);
                break;
            case R.id.radioOffScreenDuplicate:
                config.setOffScreenMode(DigitalPenConfig.DP_OFF_SCREEN_MODE_DUPLICATE);
                break;
            default:
                throw new RuntimeException("Unknown ID:" + v.getId());
        }
        setGlobalConfig();
        updateUi();
    }

    public void onClickRadioPowerMode(View v) {
        switch (v.getId()) {
            case R.id.radioPowerModeAccuracy:
                config.setPowerSave(DigitalPenConfig.DP_POWER_PROFILE_OPTIMIZE_ACCURACY);
                break;
            case R.id.radioPowerModePower:
                config.setPowerSave(DigitalPenConfig.DP_POWER_PROFILE_OPTIMIZE_POWER);
                break;
            default:
                throw new RuntimeException("Unknown ID:" + v.getId());
        }
        setGlobalConfig();
        updateUi();
    }

    public void onClickRadioOffScreenLocation(View v) {
        switch (v.getId()) {
            case R.id.radioOffScreenLocationLeft:
                config.setOffScreenPortraitSide(DigitalPenConfig.DP_PORTRAIT_SIDE_LEFT);
                break;
            case R.id.radioOffScreenLocationRight:
                config.setOffScreenPortraitSide(DigitalPenConfig.DP_PORTRAIT_SIDE_RIGHT);
                break;
            default:
                throw new RuntimeException("Unknown ID:" + v.getId());
        }
        setGlobalConfig();
        updateUi();
    }

    public void onClickRadioEraserBehavior(View v) {
        switch (v.getId()) {
            case R.id.radioEraserBehaviorHold:
                config.setEraseButtonBehavior(DigitalPenConfig.DP_ERASE_MODE_HOLD);
                break;
            case R.id.radioEraserBehaviorToggle:
                config.setEraseButtonBehavior(DigitalPenConfig.DP_ERASE_MODE_TOGGLE);
                break;
            default:
                throw new RuntimeException("Unknown ID:" + v.getId());
        }
        setGlobalConfig();
        updateUi();
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (initializingWidgetValues) {
            return; // button set due to resume or create, not user action
        }
        switch (buttonView.getId()) {

            case R.id.switchEraserEnable:
                // the edit text done handler checks this button state and
                // handles config change.
                integerTextFieldDone(eraseButtonIndex);
                return; // all other buttons fall-through to loadConfig
            case R.id.switchSmarterStand:
                config.setSmarterStand(isChecked);
                break;
            case R.id.switchStopPenOnScreenOff:
                config.setStopPenOnScreenOff(isChecked);
                break;
            case R.id.switchStartPenOnBoot:
                config.setStartPenOnBoot(isChecked);
                break;
            case R.id.switchOnScreenHover:
                config.setOnScreenHoverEnable(isChecked);
                break;
            case R.id.switchOffScreenHover:
                config.setOffScreenHoverEnable(isChecked);
                break;
            case R.id.switchOnScreenHoverIcon:
                config.setShowOnScreenHoverIcon(isChecked);
                break;
            case R.id.switchOffScreenHoverIcon:
                config.setShowOffScreenHoverIcon(isChecked);
                break;
            default:
                throw new RuntimeException("Unknown switch, id: " + buttonView.getId());
        }
        setGlobalConfig();
        updateUi();
    }

    public void onClickEraserEnable(View v) {
        // the edit text done handler checks this button state
        integerTextFieldDone(eraseButtonIndex);
    }

}
