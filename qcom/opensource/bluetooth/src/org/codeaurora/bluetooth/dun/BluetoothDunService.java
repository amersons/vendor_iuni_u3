/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *            notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *            notice, this list of conditions and the following disclaimer in the
 *            documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *            the names of its contributors may be used to endorse or promote
 *            products derived from this software without specific prior written
 *            permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.codeaurora.bluetooth.dun;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothDun;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothDun;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import android.bluetooth.BluetoothAdapter;
import android.os.SystemProperties;
import android.app.Service;
import android.bluetooth.IBluetooth;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import java.io.OutputStream;
import java.io.InputStream;
import android.os.IBinder;
import java.nio.ByteBuffer;
import java.io.IOException;
import android.content.ServiceConnection;
import android.os.ParcelUuid;
import java.util.UUID;
import android.text.TextUtils;
import android.content.ComponentName;
import android.os.RemoteException;
import org.codeaurora.bluetooth.R;

/**
 * Provides Bluetooth Dun profile, as a service in the BluetoothExt APK.
 * @hide
 */

public class BluetoothDunService extends Service {

    private static final String TAG                             = "BluetoothDunService";
    private static final boolean DBG                            = false;
    public  static final boolean VERBOSE                        = false;

    private final static String DUN_SERVER                      = "qcom.dun.server";

    private static final int MESSAGE_START_LISTENER             = 0x01;
    private static final int MESSAGE_DUN_USER_TIMEOUT           = 0x02;

    private static final int USER_CONFIRM_TIMEOUT_VALUE         = 30000; // 30 seconds

    private static final int MON_THREAD_SLEEP_INTERVAL          = 200; // 200 mseconds

    /**
     * IPC message types which can be exchanged between
     * Dun server process and BluetoothDunService
     */
    private static final byte DUN_IPC_MSG_DUN_REQUEST           = 0x00;
    private static final byte DUN_IPC_MSG_DUN_RESPONSE          = 0x01;
    private static final byte DUN_IPC_MSG_CTRL_REQUEST          = 0x02;
    private static final byte DUN_IPC_MSG_CTRL_RESPONSE         = 0x03;
    private static final byte DUN_IPC_MSG_MDM_STATUS            = 0x04;

    /**
     * Control message types between Dun server and
     * BluetoothDunService
     */
    private static final byte DUN_CRTL_MSG_DISCONNECT_REQ       = 0x00;
    private static final byte DUN_CRTL_MSG_CONNECTED_RESP       = 0x01;
    private static final byte DUN_CRTL_MSG_DISCONNECTED_RESP    = 0x02;

    /**
     * Offsets of message data with in the IPC message
     */
    private static final byte DUN_IPC_MSG_OFF_MSG_TYPE          = 0x00;
    private static final byte DUN_IPC_MSG_OFF_MSG_LEN           = 0x01;
    private static final byte DUN_IPC_MSG_OFF_MSG               = 0x03;

    /**
     * Server supported DUN Profile's maximum message size
     */
    private static final int DUN_MAX_MSG_LEN                    = 32764;

    /**
     * IPC message's header size
     */
    private static final int DUN_IPC_HEADER_SIZE                = 0x03;

    /**
     * IPC message's maximum message size (Dun server <---> DUN service)
     */
    private static final int DUN_MAX_IPC_MSG_LEN                = DUN_MAX_MSG_LEN +
                                                                  DUN_IPC_HEADER_SIZE;
    /**
     * IPC message's control message size
     */
    private static final byte DUN_IPC_CTRL_MSG_SIZE             = 0x01;

    /**
     * IPC message's modem status message size
     */
    private static final byte DUN_IPC_MDM_STATUS_MSG_SIZE       = 0x01;

    /**
     * BT socket options
     */
    private static final int BTSOCK_OPT_GET_MODEM_BITS          = 0x01;
    private static final int BTSOCK_OPT_SET_MODEM_BITS          = 0x02;
    private static final int BTSOCK_OPT_CLR_MODEM_BITS          = 0x03;

    private static IBluetooth mAdapterService                   = null;

    private static BluetoothDevice mRemoteDevice                = null;

    private static volatile SocketAcceptThread mAcceptThread    = null;

    /**
     * Thread for reading the requests from DUN client using the socket
     * mRfcommSocket and upload the same requests to DUNd using the
     * socket mDundSocket
     */
    private static volatile UplinkThread mUplinkThread          = null;

    /**
     * Thtread for reading(download) the responses from DUNd using
     * the socket mDundSocket and route the same responses to
     * DUN client using the socket mRfcommSocket
     */
    private static volatile DownlinkThread mDownlinkThread      = null;

    /**
     * Thtread for monitoring the modem status and base on the status
     * chnage it will update the DUN server with the updated status
     */
    private static volatile MonitorThread mMonitorThread      = null;

    /**
     * Listening Rfcomm Socket
     */
    private static volatile BluetoothServerSocket mListenSocket = null;

    /**
     * Connected Rfcomm socket
     */
    private static volatile BluetoothSocket mRfcommSocket       = null;

    /**
     * Socket represents the connection between DUNd and DUN service
     */
    private static volatile LocalSocket mDundSocket             = null;

    private boolean mIsWaitingAuthorization                     = false;

    private volatile boolean mInterrupted                       = false;

    private boolean mDunEnable                                  = false;

    private byte mRmtMdmStatus                                  = 0x00;

    /**
     * DUN profile's UUID
     */
    private static final String DUN_UUID = "00001103-0000-1000-8000-00805F9B34FB";

    /**
     * Intent indicating incoming connection request which is sent to
     * BluetoothDunPermissionActivity
     */
    public static final String DUN_ACCESS_REQUEST_ACTION =
                                     "org.codeaurora.bluetooth.dun.accessrequest";

    /**
     * Intent indicating incoming connection request accepted by user which is
     * sent from BluetoothDunPermissionActivity
     */
    public static final String DUN_ACCESS_ALLOWED_ACTION =
                                     "org.codeaurora.bluetooth.dun.accessallowed";

    /**
     * Intent indicating incoming connection request denied by user which is
     * sent from BluetoothDunPermissionActivity
     */
    public static final String DUN_ACCESS_DISALLOWED_ACTION =
                                  "org.codeaurora.bluetooth.dun.accessdisallowed";

    /**
     * Intent indicating timeout for user confirmation, which is sent to
     * BluetoothDunPermissionActivity
     */
    public static final String USER_CONFIRM_TIMEOUT_ACTION =
                                "org.codeaurora.bluetooth.dun.userconfirmtimeout";

    public static final String THIS_PACKAGE_NAME = "org.codeaurora.bluetooth";

    /**
     * Intent Extra name indicating always allowed which is sent from
     * BluetoothDunPermissionActivity
     */
    public static final String DUN_EXTRA_ALWAYS_ALLOWED =
                                     "org.codeaurora.bluetooth.dun.alwaysallowed";

    // Ensure not conflict with Opp notification ID
    private static final int DUN_NOTIFICATION_ID_ACCESS = -1000006;

    /**
     * Intent Extra name indicating BluetoothDevice which is sent to
     * BluetoothDunPermissionActivity
     */
    public static final String EXTRA_BLUETOOTH_DEVICE =
                                   "org.codeaurora.bluetooth.dun.bluetoothdevice";

    /**
     * String signifies the DUN profile status
     */
    public static final String BLUETOOTH_DUN_PROFILE_STATUS = "bluetooth.dun.status";

    public static final ParcelUuid DUN = ParcelUuid.fromString(DUN_UUID);

    private static final Object mAcceptLock = new Object();

    private static final Object mUplinkLock = new Object();

    private static final Object mDownlinkLock = new Object();

    private static final Object mMonitorLock = new Object();

    private static final Object mAuthLock = new Object();

    private HashMap<BluetoothDevice, BluetoothDunDevice> mDunDevices;

    private IBinder mDunBinder;

    private BluetoothAdapter mAdapter;

    /**
     * package and class name to which we send intent to check DUN profile
     * access permission
     */
    private static final String ACCESS_AUTHORITY_PACKAGE = "com.android.settings";
    private static final String ACCESS_AUTHORITY_CLASS =
        "com.android.settings.bluetooth.BluetoothPermissionRequest";

    private static final String BLUETOOTH_ADMIN_PERM =
                    android.Manifest.permission.BLUETOOTH_ADMIN;

    private static final String BLUETOOTH_PERM =
                    android.Manifest.permission.BLUETOOTH;

    @Override
    public void onCreate() {

        super.onCreate();

        if (VERBOSE) Log.v(TAG, "Dun Service onCreate");

        mDunDevices = new HashMap<BluetoothDevice, BluetoothDunDevice>();

        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mDunBinder = initBinder();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        mInterrupted = false;

        if (mAdapter == null) {
            Log.w(TAG, "Stopping BluetoothDunService: "
                    + "device does not have BT or device is not ready");
            // Release all resources
            closeDunService();
        } else {
            if (intent != null) {
                parseIntent(intent);
            }
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (VERBOSE) Log.v(TAG, "Dun Service onDestroy");

        super.onDestroy();

        if(mDunDevices != null)
            mDunDevices.clear();

        closeDunService();
        if(mDunHandler != null) {
            mDunHandler.removeCallbacksAndMessages(null);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (VERBOSE) Log.v(TAG, "Dun Service onBind");
        return mDunBinder;
    }

    private IBinder initBinder() {
        return new BluetoothDunBinder(this);
    }

    private final Handler mDunHandler = new Handler() {
        @Override
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case MESSAGE_START_LISTENER:
                        if (mAdapter.isEnabled()) {
                            startRfcommListenerThread();
                        }
                        break;
                    case MESSAGE_DUN_USER_TIMEOUT:
                        {
                            synchronized(mAuthLock) {
                                if (!mIsWaitingAuthorization) {
                                    // already handled, ignore it
                                    break;
                                }

                                mIsWaitingAuthorization = false;
                            }
                            Intent intent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
                            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mRemoteDevice);
                            sendBroadcast(intent);
                            removeDunNotification(DUN_NOTIFICATION_ID_ACCESS);
                            /* close the rfcomm socket and restart the listener thread */
                            closeRfcommSocket();
                            startRfcommListenerThread();

                            if (DBG) Log.d(TAG,"DUN user Authorization Timeout");
                        }
                        break;
                }
            }
    };

    // process the intent from DUN receiver
    private void parseIntent(final Intent intent) {

        String action = intent.getStringExtra("action");
        if (VERBOSE) Log.v(TAG, "action: " + action);

        int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
        if (VERBOSE) Log.v(TAG, "state: " + state);

        boolean removeTimeoutMsg = true;
        if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {

            if ((state == BluetoothAdapter.STATE_ON) && (!mDunEnable)) {

                synchronized(mConnection) {
                    try {
                        if (mAdapterService == null) {
                            if ( !bindService(new Intent(IBluetooth.class.getName()), mConnection, 0)) {
                                Log.v(TAG, "Could not bind to AdapterService");
                                return;
                            }
                        }
                    } catch (Exception e) {
                        Log.e(TAG, "bindService Exception", e);
                        return;
                    }
                }

                Log.v(TAG, "Starting DUN server process");
                try {
                    SystemProperties.set(BLUETOOTH_DUN_PROFILE_STATUS, "running");
                    mDunEnable = true;
                } catch (RuntimeException e) {
                    Log.v(TAG, "Could not start DUN server process: " + e);
                }

                if (mDunEnable) {
                    mDunHandler.sendMessage(mDunHandler
                            .obtainMessage(MESSAGE_START_LISTENER));
                } else {
                    //DUN server process is not started successfully.
                    //So clean up service connection to avoid service connection leak
                    if (mAdapterService != null) {
                        try {
                            mAdapterService = null;
                            unbindService(mConnection);
                        } catch (IllegalArgumentException e) {
                            Log.e(TAG, "could not unbind the adapter Service", e);
                        }
                    }
                    return;
                }
            }
            else if (state == BluetoothAdapter.STATE_TURNING_OFF) {
                // Send any pending timeout now, as this service will be destroyed.
                if (mDunHandler.hasMessages(MESSAGE_DUN_USER_TIMEOUT)) {
                    Intent timeoutIntent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_CANCEL);
                    timeoutIntent.setClassName(ACCESS_AUTHORITY_PACKAGE, ACCESS_AUTHORITY_CLASS);
                    sendBroadcast(timeoutIntent, BLUETOOTH_ADMIN_PERM);
                }

                if (mDunEnable) {
                    Log.v(TAG, "Stopping DUN server process");
                    try {
                        SystemProperties.set(BLUETOOTH_DUN_PROFILE_STATUS, "stopped");
                    } catch (RuntimeException e) {
                        Log.v(TAG, "Could not stop DUN server process: " + e);
                    }

                    synchronized(mConnection) {
                        try {
                            mAdapterService = null;
                            unbindService(mConnection);
                        } catch (Exception e) {
                            Log.e(TAG, "", e);
                        }
                    }

                    // Release all resources
                    closeDunService();
                    mDunEnable = false;
                }
                synchronized(mAuthLock) {
                    if (mIsWaitingAuthorization) {
                        removeDunNotification(DUN_NOTIFICATION_ID_ACCESS);
                        mIsWaitingAuthorization = false;
                    }
                }

            } else {
                removeTimeoutMsg = false;
            }
        } else if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED) &&
                                 mIsWaitingAuthorization) {
            if (mRemoteDevice == null) {
               Log.e(TAG, "Unexpected error!");
               return;
            }
            if (mDunHandler != null) {
               /* Let the user timeout handle this case as well */
               mDunHandler.sendMessage(mDunHandler.obtainMessage(MESSAGE_DUN_USER_TIMEOUT));
               removeTimeoutMsg = false;
            }

        }  else if (action.equals(DUN_ACCESS_ALLOWED_ACTION)) {
            if (mRemoteDevice == null) {
               Log.e(TAG, "Unexpected error!");
               return;
            }

            if (VERBOSE) {

                Log.v(TAG, "DunService-Received ACCESS_ALLOWED_ACTION");
            }
            synchronized(mAuthLock) {
                if (!mIsWaitingAuthorization) {
                    // this reply is not for us
                    return;
                }

                mIsWaitingAuthorization = false;
            }

            if (intent.getBooleanExtra(BluetoothDunService.DUN_EXTRA_ALWAYS_ALLOWED, false) == true) {
                  if(mRemoteDevice != null) {
                     mRemoteDevice.setTrust(true);
                     Log.v(TAG, "setTrust() TRUE " + mRemoteDevice.getName());
                  }
            }
            /* start the uplink thread */
            startUplinkThread();

        } else if (action.equals(DUN_ACCESS_DISALLOWED_ACTION)) {
            /* close the rfcomm socket and restart the listener thread */
            Log.d(TAG,"DUN_ACCESS_DISALLOWED_ACTION:" + mIsWaitingAuthorization);

            mIsWaitingAuthorization = false;
            closeRfcommSocket();
            startRfcommListenerThread();
        } else if ( BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(action)) {

            if (intent.hasExtra(BluetoothDevice.EXTRA_DEVICE)) {
               BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if(device != null)
                    Log.d(TAG,"device: "+ device.getName());
                if(mRemoteDevice != null)
                    Log.d(TAG," Remtedevie: "+mRemoteDevice.getName());
               if ((device != null) && (mRemoteDevice != null) && (device.equals(mRemoteDevice)) &&
                    (mRemoteDevice.getTrustState()) && (intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                               BluetoothDevice.BOND_NONE) == BluetoothDevice.BOND_NONE)) {

                   Log.d(TAG,"BOND_STATE_CHANGED REFRESH trustDevices"+ device.getName());
                   mRemoteDevice.setTrust(false);
               }
            }

        } else {
            removeTimeoutMsg = false;
        }

        if (removeTimeoutMsg) {
            mDunHandler.removeMessages(MESSAGE_DUN_USER_TIMEOUT);
        }
    }

    private void startRfcommListenerThread() {
        if (VERBOSE) Log.v(TAG, "DUN Service startRfcommListenerThread");

        synchronized(mAcceptLock) {
            // if it is already running,close it
            if (mAcceptThread != null) {
                try {
                    mAcceptThread.shutdown();
                    mAcceptThread.join();
                    mAcceptThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mAcceptThread close error" + ex);
                }
            }
            if (mAcceptThread == null) {
                mAcceptThread = new SocketAcceptThread();
                mAcceptThread.setName("BluetoothDunAcceptThread");
                mAcceptThread.start();
            }
        }
    }

    private void startUplinkThread() {
        if (VERBOSE) Log.v(TAG, "DUN Service startUplinkThread");

        synchronized(mUplinkLock) {
            if (mUplinkThread != null) {
                try {
                    mUplinkThread.shutdown();
                    mUplinkThread.join();
                    mUplinkThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mUplinkThread close error" + ex);
                }
            }
            if (mUplinkThread == null) {
                mUplinkThread = new UplinkThread();
                mUplinkThread.setName("BluetoothDunUplinkThread");
                mUplinkThread.start();
            }
        }
    }

    private void startDownlinkThread() {
        if (VERBOSE) Log.v(TAG, "DUN Service startDownlinkThread");

        synchronized(mDownlinkLock) {
            if (mDownlinkThread == null) {
                mDownlinkThread = new DownlinkThread();
                mDownlinkThread.setName("BluetoothDunDownlinkThread");
                mDownlinkThread.start();
            }
        }
    }

    private void startMonitorThread() {
        if (VERBOSE) Log.v(TAG, "DUN Service startMonitorThread");

        synchronized(mMonitorLock) {
            if (mMonitorThread  == null) {
                mMonitorThread  = new MonitorThread();
                mMonitorThread .setName("BluetoothDunMonitorThread");
                mMonitorThread .start();
            }
        }
    }

    private final boolean initRfcommSocket() {
        if (VERBOSE) Log.v(TAG, "Dun Service initRfcommSocket");

        boolean initRfcommSocketOK = false;
        final int CREATE_RETRY_TIME = 10;

        // It's possible that create will fail in some cases. retry for 10 times
        for (int i = 0; i < CREATE_RETRY_TIME && !mInterrupted; i++) {
            initRfcommSocketOK = true;
            try {
                // start listening for incoming rfcomm channel connection
                mListenSocket = mAdapter.listenUsingRfcommWithServiceRecord("Dial up Networking", DUN.getUuid());

            } catch (IOException e) {
                Log.e(TAG, "Error create RfcommListenSocket " + e.toString());
                initRfcommSocketOK = false;
            }
            if (!initRfcommSocketOK) {
                // Need to break out of this loop if BT is being turned off.
                if (mAdapter == null) break;
                int state = mAdapter.getState();
                if ((state != BluetoothAdapter.STATE_TURNING_ON) && (state != BluetoothAdapter.STATE_ON)) {
                    Log.w(TAG, "initRfcommSocket failed as BT is (being) turned off");
                    break;
                }
                try {
                    if (VERBOSE) Log.v(TAG, "wait 300 ms");
                    Thread.sleep(300);
                } catch (InterruptedException e) {
                    Log.e(TAG, "socketAcceptThread thread was interrupted (3)");
                    break;
                }
            } else {
                break;
            }
        }

        if (mInterrupted) {
            initRfcommSocketOK = false;
            //close the listen socket to avoid resource leakage
            closeListenSocket();
        }

        if (initRfcommSocketOK) {
            if (VERBOSE) Log.v(TAG, "Succeed to create listening socket ");

        } else {
            Log.e(TAG, "Error to create listening socket after " + CREATE_RETRY_TIME + " try");
        }
        return initRfcommSocketOK;
    }

    private final boolean initDundClientSocket() {
        if (VERBOSE) Log.v(TAG, "DUN initDundClientSocket");

        boolean initDundSocketOK = false;

        try {
            LocalSocketAddress locSockAddr = new LocalSocketAddress(DUN_SERVER);
            mDundSocket = new LocalSocket();
            mDundSocket.connect(locSockAddr);
            initDundSocketOK = true;
        } catch (java.io.IOException e) {
            Log.e(TAG, "cant connect: " + e);
        }

        if (initDundSocketOK) {
            if (VERBOSE) Log.v(TAG, "Succeed to create Dund socket ");

        } else {
            Log.e(TAG, "Error to create Dund socket");
        }
        return initDundSocketOK;
    }

    private final synchronized void closeListenSocket() {
        // exit SocketAcceptThread early
        if (VERBOSE) Log.v(TAG, "Close listen Socket : " );
        if (mListenSocket != null) {
            try {
                // this will cause mListenSocket.accept() return early with
                // IOException
                mListenSocket.close();
                mListenSocket = null;
            } catch (IOException ex) {
                Log.e(TAG, "Close listen Socket error: " + ex);
            }
        }
    }

    private final synchronized void closeRfcommSocket() {
        if (VERBOSE) Log.v(TAG, "Close rfcomm Socket : " );
        if (mRfcommSocket != null) {
            try {
                mRfcommSocket.close();
                mRfcommSocket = null;
            } catch (IOException e) {
                Log.e(TAG, "Close Rfcomm Socket error: " + e.toString());
            }
        }
    }

    private final synchronized void closeDundSocket() {
        if (VERBOSE) Log.v(TAG, "Close dund  Socket : " );
        if (mDundSocket != null) {
            try {
                mDundSocket.close();
                mDundSocket = null;
            } catch (IOException e) {
                Log.e(TAG, "Close Dund Socket error: " + e.toString());
            }
        }
    }

    private final void closeDunService() {
        if (VERBOSE) Log.v(TAG, "Dun Service closeDunService in");

        // exit initRfcommSocket early
        mInterrupted = true;

        closeListenSocket();

        closeRfcommSocket();

        closeDundSocket();

        synchronized(mMonitorLock) {
            if (mMonitorThread != null) {
                try {
                    mMonitorThread.shutdown();
                    mMonitorThread.join();
                    mMonitorThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mMonitorThread close error" + ex);
                }
            }
        }

        synchronized(mDownlinkLock) {
            if (mDownlinkThread != null) {
                try {
                    mDownlinkThread.shutdown();
                    mDownlinkThread.join();
                    mDownlinkThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mDownlinkThread close error" + ex);
                }
            }
        }

        synchronized(mUplinkLock) {
            if (mUplinkThread != null) {
                try {
                    mUplinkThread.shutdown();
                    mUplinkThread.join();
                    mUplinkThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mUplinkThread close error" + ex);
                }
            }
        }
        /* remove the MESSAGE_START_LISTENER message which might be posted
         * by the uplink thread */
        mDunHandler.removeMessages(MESSAGE_START_LISTENER);

        synchronized(mAcceptLock) {
            if (mAcceptThread != null) {
                try {
                    mAcceptThread.shutdown();
                    mAcceptThread.join();
                    mAcceptThread = null;
                } catch (InterruptedException ex) {
                    Log.w(TAG, "mAcceptThread close error" + ex);
                }
            }
        }

        stopSelf();

        if (VERBOSE) Log.v(TAG, "Dun Service closeDunService out");
    }

    private void createDunNotification(BluetoothDevice device) {
        if (VERBOSE) Log.v(TAG, "Creating DUN access notification");

        NotificationManager nm = (NotificationManager)
                getSystemService(Context.NOTIFICATION_SERVICE);

        // Create an intent triggered by clicking on the status icon.
        Intent clickIntent = new Intent();
        clickIntent.setClass(this, BluetoothDunPermissionActivity.class);
        clickIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        clickIntent.setAction(DUN_ACCESS_REQUEST_ACTION);
        clickIntent.putExtra(EXTRA_BLUETOOTH_DEVICE, device);

        // Create an intent triggered by clicking on the
        // "Clear All Notifications" button
        Intent deleteIntent = new Intent();
        deleteIntent.setClass(this, BluetoothDunReceiver.class);

        Notification notification = null;
        String name = device.getName();
        if (TextUtils.isEmpty(name)) {
            name = getString(R.string.defaultname);
        }

        deleteIntent.setAction(DUN_ACCESS_DISALLOWED_ACTION);
        notification = new Notification(android.R.drawable.stat_sys_data_bluetooth,
            getString(R.string.dun_notif_ticker), System.currentTimeMillis());
        notification.setLatestEventInfo(this, getString(R.string.dun_notif_ticker),
                getString(R.string.dun_notif_message, name), PendingIntent
                        .getActivity(this, 0, clickIntent, 0));

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        notification.flags |= Notification.FLAG_ONLY_ALERT_ONCE;
        notification.defaults = Notification.DEFAULT_SOUND;
        notification.deleteIntent = PendingIntent.getBroadcast(this, 0, deleteIntent, 0);
        nm.notify(DUN_NOTIFICATION_ID_ACCESS, notification);


        if (VERBOSE) Log.v(TAG, "Awaiting Authorization : DUN Connection : " + device.getName());

    }

    private void removeDunNotification(int id) {
        Context context = getApplicationContext();
        NotificationManager nm = (NotificationManager) context
                .getSystemService(Context.NOTIFICATION_SERVICE);
        nm.cancel(id);
    }

    /**
     * A thread that runs in the background waiting for remote rfcomm
     * connect.Once a remote socket connected, this thread shall be
     * shutdown.When the remote disconnect,this thread shall run again
     * waiting for next request.
     */
    private class SocketAcceptThread extends Thread {

        private boolean stopped = false;

        @Override
        public void run() {
            BluetoothServerSocket listenSocket;
            if (mListenSocket == null) {
                if (!initRfcommSocket()) {
                    return;
                }
            }

            while (!stopped) {
                try {
                    if (VERBOSE) Log.v(TAG, "Accepting socket connection...");
                    listenSocket = mListenSocket;
                    if (listenSocket == null) {
                        Log.w(TAG, "mListenSocket is null");
                        break;
                    }

                    mRfcommSocket = mListenSocket.accept();

                    synchronized (BluetoothDunService.this) {
                        if (mRfcommSocket == null) {
                            Log.w(TAG, " mRfcommSocket is null");
                            break;
                        }
                        mRemoteDevice = mRfcommSocket.getRemoteDevice();
                    }
                    if (mRemoteDevice == null) {
                        /* close the rfcomm socket */
                        closeRfcommSocket();
                        Log.i(TAG, "getRemoteDevice() = null");
                        break;
                    }
                    boolean trust = false;
                    if (mRemoteDevice != null)
                        trust = mRemoteDevice.getTrustState();
                    if (VERBOSE) Log.v(TAG, "GetTrustState() = " + trust);

                    if (trust) {
                        /* start the uplink thread */
                        startUplinkThread();
                    } else {
                        createDunNotification(mRemoteDevice);

                        if (VERBOSE) Log.v(TAG, "incoming connection accepted from: "+ mRemoteDevice);
                        mIsWaitingAuthorization = true;

                        mDunHandler.sendMessageDelayed(mDunHandler.obtainMessage(MESSAGE_DUN_USER_TIMEOUT),
                                 USER_CONFIRM_TIMEOUT_VALUE);
                    }
                    stopped = true; // job done ,close this thread;
                    if (VERBOSE) Log.v(TAG, "SocketAcceptThread stopped ");
                } catch (IOException ex) {
                    stopped=true;
                    if (VERBOSE) Log.v(TAG, "Accept thread exception: " + ex.toString());
                }
            }
        }

        void shutdown() {
            stopped = true;
            interrupt();
        }
    }

    /**
     * A thread that runs in the background waiting for remote DUNd socket
     * connection.Once the connection is established, this thread starts the
     * downlink thread and starts forwarding the DUN profile requests to
     * the DUNd on receiving the requests from rfcomm channel.
     */
    private class UplinkThread extends Thread {

        private boolean stopped = false;
        private boolean IntExit = false;
        private OutputStream mDundOutputStream = null;
        private InputStream mRfcommInputStream = null;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(DUN_MAX_IPC_MSG_LEN);
        private int NumRead = 0;
        private byte TempByte = 0;

        @Override
        public void run() {
            if (mDundSocket == null) {
                if (!initDundClientSocket()) {
                    /* close the rfcomm socket to avoid resource leakage */
                    closeRfcommSocket();
                    /*restart the listener thread */
                    mDunHandler.sendMessage(mDunHandler.obtainMessage(MESSAGE_START_LISTENER));
                    return;
                }
            }
            if(mDundSocket != null) {
                if( mDundSocket.isConnected())
                    try {
                        mDundOutputStream = mDundSocket.getOutputStream();
                    } catch (IOException ex) {
                        if (VERBOSE)
                            Log.v(TAG, "mDundOutputStream exception: " + ex.toString());
                    }
                else return;
            }
            else return;

            if (mRfcommSocket != null) {
                if(mRfcommSocket.isConnected())
                    try {
                        mRfcommInputStream = mRfcommSocket.getInputStream();
                    } catch (IOException ex) {
                        if (VERBOSE)
                            Log.v(TAG, "mRfcommInputStream exception: " + ex.toString());
                    }
                else return;
            }
            else return;

            // create the downlink thread for reading the responses
            //from DUN server process
            startDownlinkThread();
            // create the modem status monitor thread
            startMonitorThread();

            while (!stopped) {
                try {
                    if (VERBOSE)
                        Log.v(TAG, "Reading the DUN requests from Rfcomm channel");
                    /* Read the DUN request from Rfcomm channel */
                    NumRead = mRfcommInputStream.read(IpcMsgBuffer.array(), DUN_IPC_MSG_OFF_MSG,
                                                                                DUN_MAX_MSG_LEN);
                    if ( NumRead < 0) {
                        IntExit = true;
                        break;
                    }
                    else if ( NumRead != 0) {
                        /* Wrtie the same DUN request to the DUN server socket with
                           some additional parameters */
                        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG_TYPE, DUN_IPC_MSG_DUN_REQUEST);
                        IpcMsgBuffer.putShort(DUN_IPC_MSG_OFF_MSG_LEN, (short)NumRead);
                        /* swap bytes of message len as buffer is Big Endian */
                        TempByte = IpcMsgBuffer.get(DUN_IPC_MSG_OFF_MSG_LEN);

                        IpcMsgBuffer.put( DUN_IPC_MSG_OFF_MSG_LEN, IpcMsgBuffer.get(DUN_IPC_MSG_OFF_MSG_LEN + 1));

                        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG_LEN + 1, TempByte);
                        try {
                            mDundOutputStream.write(IpcMsgBuffer.array(), 0, NumRead + DUN_IPC_HEADER_SIZE);
                        } catch (IOException ex) {
                            break;
                        }
                    }
                } catch (IOException ex) {
                    IntExit = true;
                    if (VERBOSE) Log.v(TAG, "Rfcomm channel Read exception: " + ex.toString());
                    break;
                }
            }
            // send the disconneciton request immediate to Dun server
            if (IntExit) {
                disconnect(mRemoteDevice);
            }

            /* close the dund socket */
            closeDundSocket();
            /* close the rfcomm socket */
            closeRfcommSocket();
            // Intimate the Settings APP about the disconnection
            handleDunDeviceStateChange(mRemoteDevice, BluetoothProfile.STATE_DISCONNECTED);

            /* wait for dun downlink thread to close */
                if (VERBOSE) Log.v(TAG, "wait for dun downlink thread to close");
            synchronized (mDownlinkLock) {
                if (mDownlinkThread != null) {
                    try {
                        mDownlinkThread.shutdown();
                        mDownlinkThread.join();
                        mDownlinkThread = null;
                    } catch (InterruptedException ex) {
                        Log.w(TAG, "mDownlinkThread close error" + ex);
                    }
                }
            }

            /* wait for dun monitor thread to close */
                if (VERBOSE) Log.v(TAG, "wait for dun monitor thread to close ");
            synchronized (mMonitorLock) {
                if (mMonitorThread != null) {
                    try {
                        mMonitorThread.shutdown();
                        mMonitorThread.join();
                        mMonitorThread = null;
                    } catch (InterruptedException ex) {
                        Log.w(TAG, "mMonitorThread  close error" + ex);
                    }
                }
            }

            if (IntExit && !stopped) {
                if (VERBOSE) Log.v(TAG, "starting the listener thread ");
                /* start the listener thread */
                mDunHandler.sendMessage(mDunHandler.obtainMessage(MESSAGE_START_LISTENER));
            }
            /* reset the modem status */
            mRmtMdmStatus = 0x00;
            if (VERBOSE) Log.v(TAG, "uplink thread exited ");
        }

        void shutdown() {
            stopped = true;
            interrupt();
        }
    }


    /**
     * A thread that runs in the background and monitors the modem status of rfcomm
     * channel and when there is a change in the modem status it will update the
     * DUN server process with the new status.
     */
    private class MonitorThread extends Thread {

        private boolean stopped = false;
        private int len = 0;
        private byte modemStatus = 0;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(DUN_IPC_MDM_STATUS_MSG_SIZE);
        private byte TempByte = 0;

        byte mdmBits = 0;

        @Override
        public void run() {

            while (!stopped) {
                try {
                    //wait for 200 ms
                    Thread.sleep(MON_THREAD_SLEEP_INTERVAL);
                } catch (InterruptedException e) {
                    Log.e(TAG, "MonitorThread thread was interrupted");
                    break;
                }
                try {
                    if ((mRfcommSocket == null) || (!mRfcommSocket.isConnected())) {
                        break;
                    }
                    len = mRfcommSocket.getSocketOpt(BTSOCK_OPT_GET_MODEM_BITS, IpcMsgBuffer.array());
                    if(len != DUN_IPC_MDM_STATUS_MSG_SIZE)  {
                        Log.e(TAG, "getSocketOpt return length of socket option mismatch:");
                        continue;
                    }
                } catch (IOException ex) {
                    if (VERBOSE) Log.v(TAG, "getSocketOpt Exception: " + ex.toString());
                }
                if( mdmBits != IpcMsgBuffer.get(0)) {
                    mdmBits = IpcMsgBuffer.get(0);
                    notifyModemStatus(mdmBits);
                }
            }
            if (VERBOSE) Log.v(TAG, "MonitorThread thread exited ");
        }

        void shutdown() {
            stopped = true;
            interrupt();
        }
    }


    /**
     * A thread that runs in the background and starts forwarding the
     * DUN profile responses received from Dund to the rfcomm channel.
     */
    private class DownlinkThread extends Thread {

        private boolean stopped = false;
        private OutputStream mRfcommOutputStream = null;
        private InputStream mDundInputStream = null;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(2*DUN_MAX_IPC_MSG_LEN);
        private int NumRead = 0;
        private int ReadIndex = 0;
        private short MsgLen = 0;
        private byte TempByte = 0;

        @Override
        public void run() {

            if (mDundSocket != null) {
                if ( mDundSocket.isConnected())
                    try {
                        mDundInputStream = mDundSocket.getInputStream();
                    } catch (IOException ex) {
                        if (VERBOSE) Log.v(TAG, "mDundInputStream exception: " + ex.toString());
                    }
                else return;
            }
            else return;

            if (mRfcommSocket != null) {
                if (mRfcommSocket.isConnected())
                    try {
                        mRfcommOutputStream = mRfcommSocket.getOutputStream();
                    } catch (IOException ex) {
                        if (VERBOSE) Log.v(TAG, "mRfcommOutputStream exception: " + ex.toString());
                    }
                else return;
            }
            else return;

            while (!stopped) {
                try {
                    if (VERBOSE) Log.v(TAG, "Reading the DUN responses from Dund");
                    /* Read the DUN responses from DUN server */
                    NumRead = mDundInputStream.read(IpcMsgBuffer.array(),0, DUN_MAX_IPC_MSG_LEN);
                    if (VERBOSE) Log.v(TAG, "NumRead" + NumRead);
                    if ( NumRead < 0) {
                        break;
                    }
                    else if( NumRead != 0) {
                        /* some times read buffer contains multiple responses */
                        do {
                            /* swap bytes of message len as buffer is Big Endian */
                            TempByte = IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN);

                            IpcMsgBuffer.put(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN, IpcMsgBuffer.get(ReadIndex +
                                         DUN_IPC_MSG_OFF_MSG_LEN + 1));

                            IpcMsgBuffer.put(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN + 1, TempByte);

                            MsgLen =  IpcMsgBuffer.getShort(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN);

                            if((ReadIndex + MsgLen + DUN_IPC_HEADER_SIZE) > NumRead) {
                                if (VERBOSE) Log.v(TAG, "Boundary condition hit");
                                mDundInputStream.read(IpcMsgBuffer.array(), NumRead , ReadIndex + MsgLen + DUN_IPC_HEADER_SIZE - NumRead);

                            }

                            if (IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG_TYPE) == DUN_IPC_MSG_DUN_RESPONSE) {
                                try {
                                    mRfcommOutputStream.write(IpcMsgBuffer.array(), ReadIndex + DUN_IPC_MSG_OFF_MSG,
                                           IpcMsgBuffer.getShort(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN));
                                } catch (IOException ex) {
                                    stopped = true;
                                    break;
                                }
                                if (VERBOSE)
                                    Log.v(TAG, "DownlinkThread Msg written to Rfcomm");
                            }
                            else if (IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG_TYPE) == DUN_IPC_MSG_CTRL_RESPONSE) {

                                if (IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG) == DUN_CRTL_MSG_CONNECTED_RESP) {
                                    handleDunDeviceStateChange(mRemoteDevice, BluetoothProfile.STATE_CONNECTED);
                                }
                                else if (IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG) == DUN_CRTL_MSG_DISCONNECTED_RESP) {
                                    handleDunDeviceStateChange(mRemoteDevice, BluetoothProfile.STATE_DISCONNECTED);
                                }
                                if (VERBOSE)
                                    Log.v(TAG, "DownlinkThread control message received");
                            }
                            else if (IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG_TYPE) == DUN_IPC_MSG_MDM_STATUS) {
                                /* handle the modem status messages */
                                handleModemStatusChange(IpcMsgBuffer.get(ReadIndex + DUN_IPC_MSG_OFF_MSG));
                                if (VERBOSE)
                                    Log.v(TAG, "DownlinkThread modem status message received");
                            }

                            ReadIndex = ReadIndex + IpcMsgBuffer.getShort(ReadIndex + DUN_IPC_MSG_OFF_MSG_LEN);
                            ReadIndex = ReadIndex + DUN_IPC_HEADER_SIZE;
                        } while(ReadIndex < NumRead);
                    }

                } catch (IOException ex) {
                    stopped = true;
                    if (VERBOSE) Log.v(TAG, "Dund Read exception: " + ex.toString());
                }
                /* reset the ReadIndex */
                ReadIndex = 0;
            }
            /* close the dund socket */
            closeDundSocket();
            /* close the rfcomm socket */
            closeRfcommSocket();

            if (VERBOSE) Log.v(TAG, "downlink thread exited ");
        }

        void shutdown() {
            stopped = true;
            interrupt();
        }
    }


    boolean disconnect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        OutputStream mDundOutputStream = null;
        int WriteLen = DUN_IPC_HEADER_SIZE + DUN_IPC_CTRL_MSG_SIZE;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(WriteLen);

        if (mDundSocket != null) {
            if ( mDundSocket.isConnected())
                try {
                    mDundOutputStream = mDundSocket.getOutputStream();
                } catch (IOException ex) {
                    if (VERBOSE) Log.v(TAG, "mDundOutputStream exception: " + ex.toString());
                }
            else return false;
        }
        else return false;

        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG_TYPE, DUN_IPC_MSG_CTRL_REQUEST);
        IpcMsgBuffer.putShort(DUN_IPC_MSG_OFF_MSG_LEN,DUN_IPC_CTRL_MSG_SIZE);
        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG, DUN_CRTL_MSG_DISCONNECT_REQ);
        try {
            mDundOutputStream.write(IpcMsgBuffer.array(), 0, WriteLen);
        } catch (IOException ex) {
            if (VERBOSE) Log.v(TAG, "mDundOutputStream wrtie exception: " + ex.toString());
        }
        return true;
    }

    boolean notifyModemStatus(byte status) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        OutputStream mDundOutputStream = null;
        int WriteLen = DUN_IPC_HEADER_SIZE + DUN_IPC_MDM_STATUS_MSG_SIZE;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(WriteLen);
        if (DBG) Log.d(TAG, "notifyModemStatus: status: " + status);

        if (mDundSocket != null) {
            if ( mDundSocket.isConnected())
                try {
                    mDundOutputStream = mDundSocket.getOutputStream();
                } catch (IOException ex) {
                    if (VERBOSE) Log.v(TAG, "mDundOutputStream exception: " + ex.toString());
                }
            else return false;
        }
        else return false;

        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG_TYPE, DUN_IPC_MSG_MDM_STATUS);
        IpcMsgBuffer.putShort(DUN_IPC_MSG_OFF_MSG_LEN,DUN_IPC_MDM_STATUS_MSG_SIZE);
        IpcMsgBuffer.put(DUN_IPC_MSG_OFF_MSG, status);
        try {
            mDundOutputStream.write(IpcMsgBuffer.array(), 0, WriteLen);
        } catch (IOException ex) {
            if (VERBOSE) Log.v(TAG, "mDundOutputStream wrtie exception: " + ex.toString());
        }
        return true;
    }

    int getConnectionState(BluetoothDevice device) {
        BluetoothDunDevice dunDevice = mDunDevices.get(device);
        if (dunDevice == null) {
            return BluetoothDun.STATE_DISCONNECTED;
        }
        return dunDevice.mState;
    }

    public void notifyProfileConnectionStateChanged(BluetoothDevice device,
            int profileId, int newState, int prevState) {
        if (mAdapterService != null) {
            try {
                mAdapterService.sendConnectionStateChange(device, profileId, newState, prevState);
            }catch (RemoteException re) {
                Log.e(TAG, "",re);
            }
        }
    }

    private final void handleDunDeviceStateChange(BluetoothDevice device, int state) {
        if (DBG) Log.d(TAG, "handleDunDeviceStateChange: device: " + device + " state: " + state);
        int prevState;
        BluetoothDunDevice dunDevice = mDunDevices.get(device);
        if (dunDevice == null) {
            prevState = BluetoothProfile.STATE_DISCONNECTED;
        } else {
            prevState = dunDevice.mState;
        }
        Log.d(TAG, "handleDunDeviceStateChange preState: " + prevState + " state: " + state);
        if (prevState == state) return;

        if (dunDevice == null) {
            dunDevice = new BluetoothDunDevice(state);
            mDunDevices.put(device, dunDevice);
        } else {
            dunDevice.mState = state;
        }

        /* Notifying the connection state change of the profile before sending
           the intent for connection state change, as it was causing a race
           condition, with the UI not being updated with the correct connection
           state. */
        Log.d(TAG, "Dun Device state : device: " + device + " State:" +
                       prevState + "->" + state);
        notifyProfileConnectionStateChanged(device, BluetoothProfile.DUN, state, prevState);
        Intent intent = new Intent(BluetoothDun.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDun.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothDun.EXTRA_STATE, state);
        sendBroadcast(intent, BLUETOOTH_PERM);

    }

    private final void handleModemStatusChange(byte status) {
        byte mdmBits = 0;
        int WriteLen = DUN_IPC_MDM_STATUS_MSG_SIZE;
        ByteBuffer IpcMsgBuffer = ByteBuffer.allocate(WriteLen);

        if (DBG) Log.d(TAG, "handleModemStatusChange  status: " + status);

        if ((mRfcommSocket == null) || (!mRfcommSocket.isConnected())) {
            return;
        }
        if(mRmtMdmStatus != status) {
            mdmBits = (byte)((~(int)mRmtMdmStatus) & ((int)mRmtMdmStatus ^ (int)status));
            if (DBG) Log.d(TAG, "handleModemStatusChange  mdmBits " + mdmBits);
            if(mdmBits > 0) {
                IpcMsgBuffer.put(0, mdmBits);
                try {
                    mRfcommSocket.setSocketOpt(BTSOCK_OPT_SET_MODEM_BITS, IpcMsgBuffer.array(), WriteLen);
                } catch (IOException ex) {
                    if (VERBOSE) Log.v(TAG, "getSocketOpt Exception: " + ex.toString());
                }
            }
            mdmBits = (byte)(((int)mRmtMdmStatus) & ((int)mRmtMdmStatus ^ (int)status));
            if (DBG) Log.d(TAG, "handleModemStatusChange  mdmBits " + mdmBits);
            if(mdmBits > 0) {
                IpcMsgBuffer.put(0, mdmBits);
                try {
                    mRfcommSocket.setSocketOpt(BTSOCK_OPT_CLR_MODEM_BITS, IpcMsgBuffer.array(), WriteLen);
                } catch (IOException ex) {
                    if (VERBOSE) Log.v(TAG, "getSocketOpt Exception: " + ex.toString());
                }
            }
            mRmtMdmStatus = status;
        }
    }

    List<BluetoothDevice> getConnectedDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        List<BluetoothDevice> devices = getDevicesMatchingConnectionStates(
                new int[] {BluetoothProfile.STATE_CONNECTED});
        return devices;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
         enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        List<BluetoothDevice> dunDevices = new ArrayList<BluetoothDevice>();

        for (BluetoothDevice device: mDunDevices.keySet()) {
            int dunDeviceState = getConnectionState(device);
            for (int state : states) {
                if (state == dunDeviceState) {
                    dunDevices.add(device);
                    break;
                }
            }
        }
        return dunDevices;
    }

    private ServiceConnection mConnection = new ServiceConnection() {
        public void onServiceConnected(ComponentName className, IBinder service) {
            mAdapterService = IBluetooth.Stub.asInterface(service);
        }

        public void onServiceDisconnected(ComponentName className) {
            mAdapterService = null;
        }
    };

    /**
     * Handlers for incoming service calls
     */
    private static class BluetoothDunBinder extends IBluetoothDun.Stub {

        private BluetoothDunService mService;

        public BluetoothDunBinder(BluetoothDunService svc) {
            mService = svc;
        }

        private BluetoothDunService getService() {
            if (mService != null)
                return mService;
            return null;
        }

        public boolean disconnect(BluetoothDevice device) {
            BluetoothDunService service = getService();
            if (service == null) return false;
            return service.disconnect(device);
        }

        public int getConnectionState(BluetoothDevice device) {
            BluetoothDunService service = getService();
            if (service == null) return BluetoothDun.STATE_DISCONNECTED;
            return service.getConnectionState(device);
        }
        public List<BluetoothDevice> getConnectedDevices() {
            BluetoothDunService service = getService();
            if (service == null) return new ArrayList<BluetoothDevice>(0);
            return service.getConnectedDevices();
        }

        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            BluetoothDunService service = getService();
            if (service == null) return new ArrayList<BluetoothDevice>(0);
            return service.getDevicesMatchingConnectionStates(states);
        }
    };

    private class BluetoothDunDevice {
        private int mState;

        BluetoothDunDevice(int state) {
            mState = state;
        }
    }
}
