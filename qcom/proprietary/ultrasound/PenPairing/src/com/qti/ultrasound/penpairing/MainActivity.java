/******************************************************************************
 * ---------------------------------------------------------------------------
 *  Copyright (c) 2013-2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 *  Qualcomm Technologies Proprietary and Confidential.
 *  ---------------------------------------------------------------------------
 *******************************************************************************/

package com.qti.ultrasound.penpairing;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;

import android.widget.Toast;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.NumberPicker;
import android.widget.Spinner;

public class MainActivity extends Activity implements OnClickListener {

    public static final int PAIRING = 0;

    public static final int PAIRED_PENS = 1;

    public static final int NUM_OF_PAIRING_TYPES = 2;

    private Context context;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        context = this;
        setContentView(R.layout.activity_main);

        Button pairedButton = (Button)findViewById(R.id.main_paired_pens_button);
        Button pairingButton = (Button)findViewById(R.id.main_pairing_button);

        pairedButton.setOnClickListener(this);
        pairingButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.main_paired_pens_button:
                startActivity(new Intent(this, PairedPensActivity.class));
                break;
            case R.id.main_pairing_button:

                LayoutInflater inflater = (LayoutInflater)getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                View penPickerView = inflater.inflate(R.layout.pen_picker, null);

                setPenSpinner(penPickerView);

                AlertDialog.Builder alert = new AlertDialog.Builder(this);
                alert.setTitle("Pairing").setView(penPickerView)
                        .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                startActivity(new Intent(context, PairingActivity.class));
                            }
                        }).setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                            }
                        });
                alert.show();
                break;
            default:
                Log.wtf(this.toString(), "View id not possible");
                finish();
                break;
        }
    }

    private void setPenSpinner(View penPickerView) {
        final Spinner choosePenSpinner = (Spinner)penPickerView.findViewById(R.id.pen_spinner);
        if (null != choosePenSpinner) {
            ArrayAdapter<String> choosePenAdapter = new ArrayAdapter<String>(context,
                    android.R.layout.simple_spinner_item, getPenNames());
            choosePenAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            choosePenSpinner.setAdapter(choosePenAdapter);
        }
        choosePenSpinner.setDescendantFocusability(Spinner.FOCUS_BLOCK_DESCENDANTS);
    }

    private String[] getPenNames() {
        AssetManager assetManager = this.getAssets();
        String[] assetsFolderslist = null;
        try {
            assetsFolderslist = assetManager.list("");
        } catch (IOException e) {
            e.printStackTrace();
        }
        ArrayList<String> penNames = new ArrayList<String>();

        for (String s : assetsFolderslist) {
            String penName = null;
            try {
                penName = readFromInputStream(assetManager.open(s + "/pen_name.txt"));
            } catch (IOException e) {
                e.printStackTrace();
            }
            if (null != penName) {
                penNames.add(penName);
            }
        }
        if (penNames.size() == 0) {
            Toast.makeText(this, "No pens available", Toast.LENGTH_SHORT).show();
            finish();
        }
        return penNames.toArray(new String[0]);
    }

    private String readFromInputStream(InputStream fileStream) {
        String fileContent = null;
        try {
            if (fileStream != null) {
                BufferedReader br = new BufferedReader(new InputStreamReader(fileStream));
                String readString = "";
                StringBuilder sb = new StringBuilder();

                while ((readString = br.readLine()) != null) {
                    sb.append(readString);
                }

                fileStream.close();
                fileContent = sb.toString();
            }
        } catch (FileNotFoundException e) {
            Log.e(this.toString(), "File not found: " + e.toString());
        } catch (IOException e) {
            Log.e(this.toString(), "Cannot read file: " + e.toString());
        }
        return fileContent;
    }
}
