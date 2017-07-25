/*
 * Copyright (c) 2008-2009, Motorola, Inc.
 * Copyright (c) 2010-2012 The Linux Foundation. All rights reserved.
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
package org.codeaurora.bluetooth.ftp;

import android.app.Service;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.ContentProviderClient;
import android.content.Intent;
import android.os.Message;
import android.util.Log;
import android.os.Handler;
import android.text.TextUtils;
import android.content.res.Resources;
import android.os.IBinder;
import android.os.Message;
import android.os.Bundle;
import android.os.ParcelUuid;
import org.codeaurora.bluetooth.R;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import java.io.IOException;
import java.util.ArrayList;
import javax.obex.ServerSession;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.content.ContentResolver;
import android.os.RemoteException;
import android.provider.MediaStore;
import javax.obex.ObexHelper;
import android.bluetooth.BluetoothUuid;


import java.util.ArrayList;
import java.util.List;
import java.util.HashSet;


public class BluetoothFtpService extends Service {
     private static final String TAG = "BluetoothFtpService";

    /**
     * To enable FTP DEBUG/VERBOSE logging - run below cmd in adb shell, and
     * restart com.android.bluetooth process. only enable DEBUG log:
     * "setprop log.tag.BluetoothFtpService DEBUG"; enable both VERBOSE and
     * DEBUG log: "setprop log.tag.BluetoothFtpService VERBOSE"
     */

    //public static final boolean DEBUG = false;

    //public static final boolean VERBOSE = false;

    public static final boolean DEBUG = true;
    public static final boolean VERBOSE = true;
    private int mState;

    /**
     * Intent indicating incoming connection request which is sent to
     * BluetoothFtpActivity
     */
    public static final String ACCESS_REQUEST_ACTION = "org.codeaurora.bluetooth.ftp.accessrequest";

    /**
     * Intent indicating incoming connection request accepted by user which is
     * sent from BluetoothFtpActivity
     */
    public static final String ACCESS_ALLOWED_ACTION = "org.codeaurora.bluetooth.ftp.accessallowed";

    /**
     * Intent indicating incoming connection request denied by user which is
     * sent from BluetoothFtpActivity
     */
    public static final String ACCESS_DISALLOWED_ACTION =
            "org.codeaurora.bluetooth.ftp.accessdisallowed";

    /**
     * Intent indicating incoming obex authentication request which is from
     * PCE(Carkit)
     */
    public static final String AUTH_CHALL_ACTION = "org.codeaurora.bluetooth.ftp.authchall";

    /**
     * Intent indicating obex session key input complete by user which is sent
     * from BluetoothFtpActivity
     */
    public static final String AUTH_RESPONSE_ACTION = "org.codeaurora.bluetooth.ftp.authresponse";

    /**
     * Intent indicating user canceled obex authentication session key input
     * which is sent from BluetoothFtpActivity
     */
    public static final String AUTH_CANCELLED_ACTION = "org.codeaurora.bluetooth.ftp.authcancelled";

    /**
     * Intent indicating timeout for user confirmation, which is sent to
     * BluetoothFtpActivity
     */
    public static final String USER_CONFIRM_TIMEOUT_ACTION =
            "org.codeaurora.bluetooth.ftp.userconfirmtimeout";

    public static final String THIS_PACKAGE_NAME = "org.codeaurora.bluetooth";

    /**
     * Intent Extra name indicating always allowed which is sent from
     * BluetoothFtpActivity
     */
    public static final String EXTRA_ALWAYS_ALLOWED = "org.codeaurora.bluetooth.ftp.alwaysallowed";

    /**
     * Intent Extra name indicating session key which is sent from
     * BluetoothFtpActivity
     */
    public static final String EXTRA_SESSION_KEY = "org.codeaurora.bluetooth.ftp.sessionkey";

    private static final String BLUETOOTH_PERM = android.Manifest.permission.BLUETOOTH;

    private static final String BLUETOOTH_ADMIN_PERM = android.Manifest.permission.BLUETOOTH_ADMIN;

    public static final int MSG_SERVERSESSION_CLOSE = 5004;

    public static final int MSG_SESSION_ESTABLISHED = 5005;

    public static final int MSG_SESSION_DISCONNECTED = 5006;

    public static final int MSG_OBEX_AUTH_CHALL = 5007;

    public static final int MSG_FILE_RECEIVED = 5008;

    public static final int MSG_FILE_DELETED = 5009;

    public static final int MSG_FILES_RECEIVED = 5010;

    public static final int MSG_FILES_DELETED = 5011;

    private static final int MSG_INTERNAL_START_LISTENER = 1;

    private static final int MSG_INTERNAL_USER_TIMEOUT = 2;

    private static final int MSG_INTERNAL_AUTH_TIMEOUT = 3;

    private static final int MSG_INTERNAL_OBEX_RFCOMM_SESSION_UP = 10;

    private static final int MSG_INTERNAL_OBEX_L2CAP_SESSION_UP = 11;

    //Port number for FTP RFComm Socket
    private static final int PORT_NUM = 20;

    private static final int DEFAULT_FTP_PSM = 5257;

    private static final int USER_CONFIRM_TIMEOUT_VALUE = 30000;

    public static final ParcelUuid FileTransfer =
            ParcelUuid.fromString("00001106-0000-1000-8000-00805f9b34fb");

    // Ensure not conflict with Opp notification ID
    private static final int NOTIFICATION_ID_ACCESS = -1000005;

    private static final int NOTIFICATION_ID_AUTH = -1000006;

    private static final int NOTIFICATION_ID_CONNECTED = -1000010;

    private static final int FTP_MEDIA_SCANNED = 4;

    private static final int FTP_MEDIA_SCANNED_FAILED = 5;

    public static final int FTP_MEDIA_ADD = 6;

    public static final int FTP_MEDIA_DELETE = 7;

    public static final int FTP_MEDIA_FILES_ADD = 8;

    public static final int FTP_MEDIA_FILES_DELETE = 9;

    public static boolean isL2capSocket = false;

    private Notification mConnectedNotification = null;

    private BluetoothAdapter mAdapter;

    private RfcommSocketAcceptThread mRfcommAcceptThread = null;

    private BluetoothFtpAuthenticator mAuth = null;

    private BluetoothServerSocket mRfcommServerSocket = null;


    private BluetoothSocket mConnSocket = null;
    private BluetoothDevice mRemoteDevice = null;

    private static String sRemoteDeviceName = null;

    private boolean mHasStarted = false;

    private volatile boolean mInterrupted;

    private int mStartId = -1;

    private BluetoothFtpObexServer mFtpServer = null;

    private ServerSession mServerSession = null;

    private boolean isWaitingAuthorization = false;


    public BluetoothFtpService() {
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (VERBOSE) Log.v(TAG, "Ftp Service onCreate");

        mAdapter = BluetoothAdapter.getDefaultAdapter();

        if (!mHasStarted) {

            int state = mAdapter.getState();
            if (VERBOSE) Log.v(TAG, "FTP service not started Adapter STATE: "+state);
            if (state == BluetoothAdapter.STATE_ON) {
                if (VERBOSE) Log.v(TAG, "FTP service start listener");
                mHasStarted = true;
                mSessionStatusHandler.sendMessage(mSessionStatusHandler
                        .obtainMessage(MSG_INTERNAL_START_LISTENER));
            }
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (VERBOSE) Log.v(TAG, "Ftp Service onStartCommand");
        int retCode = super.onStartCommand(intent, flags, startId);
        if (retCode == START_STICKY) {
            mStartId = startId;
            if (mAdapter == null) {
                Log.w(TAG, "Stopping BluetoothFtpService: "
                        + "device does not have BT or device is not ready");
                // Release all resources
                closeService();
            } else {
                // No need to handle the null intent case, because we have
                // all restart work done in onCreate()
                if (intent != null) {
                    parseIntent(intent);
                }
            }
        }
        return retCode;
    }

    // process the intent from receiver
    private void parseIntent(final Intent intent) {
        String action = (intent == null) ? null : intent.getStringExtra("action");
        if (action == null) {
            Log.e(TAG, "Unexpected error! action is null");
            return;
        }
        if (VERBOSE) Log.v(TAG, "PARSE INTENT action: " + action);

        int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
        boolean removeTimeoutMsg = true;
        if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
            removeTimeoutMsg = false;
            if ((state == BluetoothAdapter.STATE_TURNING_OFF) && (mAdapter != null)
                && (state == mAdapter.getState())) {
                /** Terminate file copy operation if it is in progress */
                FileUtils.interruptFileCopy = true;
                // Send any pending timeout now, as this service will be destroyed.
                if (mSessionStatusHandler.hasMessages(MSG_INTERNAL_USER_TIMEOUT)) {
                    Intent i = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
                    sendBroadcast(i);
                    removeFtpNotification(NOTIFICATION_ID_ACCESS);
                }
                // Release all resources
                closeService();
            } else if (state == BluetoothAdapter.STATE_ON && !mHasStarted ) {

                if (VERBOSE) Log.v(TAG, "FTP service start listener");
                mHasStarted = true;
                mSessionStatusHandler.sendMessage(mSessionStatusHandler
                        .obtainMessage(MSG_INTERNAL_START_LISTENER));
           }
        } else if (action.equals(ACCESS_ALLOWED_ACTION)) {
            if (!isWaitingAuthorization) {
                // this reply is not for us
                return;
            }

            isWaitingAuthorization = false;

                if (intent.getBooleanExtra(BluetoothFtpService.EXTRA_ALWAYS_ALLOWED, false)) {
                    if(mRemoteDevice != null) {
                       mRemoteDevice.setTrust(true);
                       Log.v(TAG, "setTrust() TRUE " + mRemoteDevice.getName());
                    }
                }
                try {
                    if (mConnSocket != null) {
                        startObexServerSession();
                    } else {
                        stopObexServerSession();
                    }
                } catch (IOException ex) {
                    Log.e(TAG, "Caught the error: " + ex.toString());
                }
            removeFtpNotification(NOTIFICATION_ID_ACCESS);
        } else if (action.equals(ACCESS_DISALLOWED_ACTION)) {
            stopObexServerSession();
        } else if (action.equals(AUTH_RESPONSE_ACTION)) {
            String sessionkey = intent.getStringExtra(EXTRA_SESSION_KEY);
            notifyAuthKeyInput(sessionkey);
            removeAuthChallTimer();
            removeFtpNotification(NOTIFICATION_ID_ACCESS);
        } else if (action.equals(AUTH_CANCELLED_ACTION)) {
            notifyAuthCancelled();
            removeAuthChallTimer();
            removeFtpNotification(NOTIFICATION_ID_ACCESS);
        } else if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED) && isWaitingAuthorization) {
            BluetoothDevice device =
                intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (device != null && device.equals(mRemoteDevice)) {
                /** Terminate file copy operation if it is in progress */
                FileUtils.interruptFileCopy = true;
                if (mSessionStatusHandler != null) {
               /* Let the user timeout handle this case as well */
                   mSessionStatusHandler.sendMessage(mSessionStatusHandler
                   .obtainMessage(MSG_INTERNAL_USER_TIMEOUT));
                   removeTimeoutMsg = false;
                 }
            }
        } else {
            removeTimeoutMsg = false;
        }

        if (removeTimeoutMsg) {
            mSessionStatusHandler.removeMessages(MSG_INTERNAL_USER_TIMEOUT);
        }
    }

    @Override
    public void onDestroy() {
        if (VERBOSE) Log.v(TAG, "Ftp Service onDestroy");

        super.onDestroy();
        closeService();
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (VERBOSE) Log.v(TAG, "Ftp Service onBind");
        return null;
    }

    private void startRfcommSocketListener() {
        if (VERBOSE) Log.v(TAG, "Ftp Service startRfcommSocketListener");

        if (mRfcommServerSocket == null) {
            if (!initRfcommSocket()) {
                closeService();
                return;
            }
        }
        if (mRfcommAcceptThread == null) {
            mRfcommAcceptThread = new RfcommSocketAcceptThread();
            mRfcommAcceptThread.setName("BluetoothFtpRfcommAcceptThread");
            mRfcommAcceptThread.start();
        }
    }
    private final boolean initRfcommSocket() {
        if (VERBOSE) Log.v(TAG, "Ftp Service initSocket");

        boolean initSocketOK = false;
        final int CREATE_RETRY_TIME = 10;

        // It's possible that create will fail in some cases. retry for 10 times
        for (int i = 0; i < CREATE_RETRY_TIME && !mInterrupted; i++) {
            try {
                // It is mandatory for PSE to support initiation of bonding and
                // encryption.
                mRfcommServerSocket = mAdapter.listenUsingRfcommWithServiceRecord("OBEX File Transfer", FileTransfer.getUuid());
                initSocketOK = true;
            } catch (IOException e) {
                Log.e(TAG, "Error create RfcommServerSocket " + e.toString());
                initSocketOK = false;
            }
            if (!initSocketOK) {
                synchronized (this) {
                    try {
                        if (VERBOSE) Log.v(TAG, "wait 300 ms");
                        Thread.sleep(300);
                    } catch (InterruptedException e) {
                        Log.e(TAG, "socketAcceptThread thread was interrupted (3)");
                        mInterrupted = true;
                    }
                }
            } else {
                break;
            }
        }

        if (initSocketOK && (mRfcommServerSocket != null) ) {
            if (VERBOSE) Log.v(TAG, "Succeed to create listening socket on channel " + PORT_NUM);

        } else {
            Log.e(TAG, "Error to create listening socket after " + CREATE_RETRY_TIME + " try");
        }
        return initSocketOK;
    }

    private final void closeRfcommSocket(boolean server, boolean accept) throws IOException {
        if (server == true) {
            // Stop the possible trying to init serverSocket
            mInterrupted = false;

            if (mRfcommServerSocket != null) {
                mRfcommServerSocket.close();
                mRfcommServerSocket =null ;
            }
        }

        if (accept == true) {
            if (mConnSocket != null) {
                mConnSocket.close();
                mConnSocket = null;
            }
        }
    }
    private final void closeService() {
        if (VERBOSE) Log.v(TAG, "Ftp Service closeService");

        try {
            closeRfcommSocket(true, false);
        } catch (IOException ex) {
            Log.e(TAG, "CloseSocket error: " + ex);
        }

        if (mRfcommAcceptThread != null) {
            try {
                mRfcommAcceptThread.shutdown();
                mRfcommAcceptThread.join();
                mRfcommAcceptThread = null;
            } catch (InterruptedException ex) {
                Log.w(TAG, "mAcceptThread close error" + ex);
            }
        }

        try {
            closeRfcommSocket(false, true);
        } catch (IOException ex) {
            Log.e(TAG, "CloseSocket error: " + ex);
        }

        if (mServerSession != null) {
            mServerSession.close();
            mServerSession = null;
        }

        mHasStarted = false;
        if (stopSelfResult(mStartId)) {
            if (VERBOSE) Log.v(TAG, "successfully stopped ftp service");
        }
    }

    private final void startObexServerSession() throws IOException {
        if (VERBOSE) Log.v(TAG, "Ftp Service startObexServerSession");

        mFtpServer = new BluetoothFtpObexServer(mSessionStatusHandler, this);
        synchronized (this) {
            mAuth = new BluetoothFtpAuthenticator(mSessionStatusHandler);
            mAuth.setChallenged(false);
            mAuth.setCancelled(false);
        }
        BluetoothFtpTransport transport;
        if(isL2capSocket == false) {
            transport = new BluetoothFtpTransport(mConnSocket,BluetoothFtpTransport.TYPE_RFCOMM);
        } else {
            transport = new BluetoothFtpTransport(mConnSocket,BluetoothFtpTransport.TYPE_L2CAP);
        }

        mServerSession = new ServerSession(transport, mFtpServer, mAuth);

        if (VERBOSE) {
            Log.v(TAG, "startObexServerSession() success!");
        }
    }

    private void stopObexServerSession() {
        if (VERBOSE) Log.v(TAG, "Ftp Service stopObexServerSession");

        if (mServerSession != null) {
            mServerSession.close();
            mServerSession = null;
        }

        mRfcommAcceptThread = null;

        try {
            closeRfcommSocket(false, true);
        } catch (IOException e) {
            Log.e(TAG, "closeSocket error: " + e.toString());
        }
        // Last obex transaction is finished, we start to listen for incoming
        // connection again
        if (mAdapter.isEnabled()) {
            startRfcommSocketListener();
        }
    }

    private void notifyAuthKeyInput(final String key) {
        synchronized (mAuth) {
            if (key != null) {
                mAuth.setSessionKey(key);
            }
            mAuth.setChallenged(true);
            mAuth.notify();
        }
    }

    private void notifyAuthCancelled() {
        synchronized (mAuth) {
            mAuth.setCancelled(true);
            mAuth.notify();
        }
    }

    private void notifyMediaScanner(Bundle obj,int op) {
        String[] mTypes = obj.getStringArray("mimetypes");
        String[] fPaths = obj.getStringArray("filepaths");
        if((op == FTP_MEDIA_ADD) || (op == FTP_MEDIA_DELETE)) {
            new FtpMediaScannerNotifier(this,obj.getString("filepath"),
                  obj.getString("mimetype"),mSessionStatusHandler,op);
        } else if (mTypes != null && fPaths != null) {
            new FtpMediaScannerNotifier(this,fPaths,
                  mTypes,mSessionStatusHandler,op);
        } else {
             Log.e(TAG, "Unexpected error! mTypes or fPaths is null");
            return;
        }
    }

    private void notifyContentResolver(Uri uri) {
        if (VERBOSE) Log.v(TAG,"FTP_MEDIA_SCANNED deleting uri "+uri);
        ContentProviderClient client = getContentResolver()
                  .acquireContentProviderClient(MediaStore.AUTHORITY);
        if (client == null) {
            Log.e(TAG, "Unexpected error! mTypes is null");
            return;
        }
        try {
            client.delete(uri, null, null);
        } catch(RemoteException e){
            Log.e(TAG,e.toString());
        }
        if (VERBOSE) Log.v(TAG,"FTP_MEDIA_SCANNED deleted uri "+uri);
    }

    /**
     * A thread that runs in the background waiting for remote rfcomm
     * connect.Once a remote socket connected, this thread shall be
     * shutdown.When the remote disconnect,this thread shall run again waiting
     * for next request.
     */
    private class RfcommSocketAcceptThread extends Thread {

        private boolean stopped = false;

        private static final String RTAG = "BluetoothFtpService:RfcommSocketAcceptThread";

        @Override
        public void run() {
            while (!stopped && mRfcommServerSocket != null) {
                try {
                    Log.v(RTAG,"Run Accept thread");
                    mConnSocket = mRfcommServerSocket.accept();
                    synchronized(BluetoothFtpService.this) {
                        if(mConnSocket == null){
                            Log.i(RTAG, "mConnSocket = null");
                            break;
                        }
                        isL2capSocket = false;
                        mRemoteDevice = mConnSocket.getRemoteDevice();
                    }
                    if (mRemoteDevice == null) {
                        Log.i(RTAG, "getRemoteDevice() = null");
                        break;
                    }
                    sRemoteDeviceName = mRemoteDevice.getName();
                    // In case getRemoteName failed and return null
                    if (TextUtils.isEmpty(sRemoteDeviceName)) {
                        sRemoteDeviceName = getString(R.string.defaultname);
                    }
                    mSessionStatusHandler.sendMessage(mSessionStatusHandler
                           .obtainMessage(MSG_INTERNAL_OBEX_RFCOMM_SESSION_UP));
                        boolean trust = false;
                    if (mRemoteDevice != null)
                       trust = mRemoteDevice.getTrustState();

                    if (VERBOSE) Log.v(RTAG, "GetTrustState() = " + trust);

                    if (trust) {
                        try {
                            Log.i(RTAG, "incomming connection accepted from: "
                                + sRemoteDeviceName + " automatically as trusted device");
                            startObexServerSession();
                        } catch (IOException ex) {
                            Log.e(RTAG, "catch exception starting obex server session"
                                    + ex.toString());
                        }
                    } else {
                        isWaitingAuthorization = true;
                        createFtpNotification(ACCESS_REQUEST_ACTION);
                        Log.i(RTAG, "waiting for authorization for connection from: "
                                + sRemoteDeviceName);
                        mSessionStatusHandler.sendMessageDelayed(mSessionStatusHandler
                                .obtainMessage(MSG_INTERNAL_USER_TIMEOUT), USER_CONFIRM_TIMEOUT_VALUE);
                    }
                    stopped = true; // job done ,close this thread;
                } catch (IOException ex) {
                    stopped = true; //IO exception, close the thread
                    //Assign socket handle to null.
                    mRfcommServerSocket = null;
                    if (VERBOSE) Log.v(RTAG, "Accept exception: " + ex.toString());
                }
            }
        }

        void shutdown() {
            Log.e(RTAG,"Shutdown");
            stopped = true;
            interrupt();
        }
    }

    private final Handler mSessionStatusHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (VERBOSE) Log.v(TAG, "Handler(): got msg=" + msg.what);

            switch (msg.what) {
                case MSG_INTERNAL_START_LISTENER:
                    if (mAdapter.isEnabled()) {
                        startRfcommSocketListener();
                    } else {
                        closeService();// release all resources
                    }
                    break;
                case MSG_INTERNAL_USER_TIMEOUT:
                    Intent intent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
                    sendBroadcast(intent);
                    removeFtpNotification(NOTIFICATION_ID_ACCESS);
                    isWaitingAuthorization = false;
                    stopObexServerSession();
                    break;
                case MSG_INTERNAL_AUTH_TIMEOUT:
                    Intent i = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
                    sendBroadcast(i);
                    removeFtpNotification(NOTIFICATION_ID_AUTH);
                    notifyAuthCancelled();
                    stopObexServerSession();
                    break;
                case MSG_SERVERSESSION_CLOSE:
                    stopObexServerSession();
                    stopForeground(true);
                    mConnectedNotification = null;
                    break;
                case MSG_SESSION_ESTABLISHED:
                    if(mConnectedNotification == null)
                        mConnectedNotification = createFtpConnectedNotification();
                    startForeground(NOTIFICATION_ID_CONNECTED, mConnectedNotification);
                    break;
                case MSG_SESSION_DISCONNECTED:
                    break;
                case MSG_FILE_RECEIVED:
                    if (VERBOSE) Log.v(TAG,"MSG_FILE_RECEIVED");
                    Bundle arguments = (Bundle) msg.obj;
                    notifyMediaScanner(arguments,FTP_MEDIA_ADD);
                    break;
                case MSG_FILE_DELETED:
                    if (VERBOSE) Log.v(TAG,"MSG_FILE_DELETED");
                    Bundle delarguments = (Bundle) msg.obj;
                    notifyMediaScanner(delarguments,FTP_MEDIA_DELETE);
                    break;
                case MSG_FILES_DELETED:
                    if (VERBOSE) Log.v(TAG,"MSG_FILES_DELETED");
                    Bundle delfilesarguments = (Bundle) msg.obj;
                    notifyMediaScanner(delfilesarguments,FTP_MEDIA_FILES_DELETE);
                    break;
                case MSG_FILES_RECEIVED:
                    if (VERBOSE) Log.v(TAG,"MSG_FILES_RECEIVED");
                    Bundle newfilearguments = (Bundle) msg.obj;
                    notifyMediaScanner(newfilearguments,FTP_MEDIA_FILES_ADD);
                    break;

                case FTP_MEDIA_SCANNED:
                    if (VERBOSE) Log.v(TAG,"FTP_MEDIA_SCANNED arg1 "+msg.arg1);
                    Uri uri = (Uri)msg.obj;
                    /* If the media scan was for a
                     * Deleted file Delete the entry
                     * from content resolver
                     */
                    if((msg.arg1 == FTP_MEDIA_DELETE) || (msg.arg1 == FTP_MEDIA_FILES_DELETE)) {
                        notifyContentResolver(uri);
                    }
                    break;
                case MSG_INTERNAL_OBEX_RFCOMM_SESSION_UP:
                    if (VERBOSE) Log.v(TAG,"MSG_INTERNAL_OBEX_RFCOMM_SESSION_UP");
                    /*Avoid RfcommServer socket close to avoid SDP
                     *re-registration for every FTP connection request
                     */
                    break;
                case MSG_OBEX_AUTH_CHALL:
                    createFtpNotification(AUTH_CHALL_ACTION);
                    mSessionStatusHandler.sendMessageDelayed(mSessionStatusHandler
                         .obtainMessage(MSG_INTERNAL_AUTH_TIMEOUT), USER_CONFIRM_TIMEOUT_VALUE);
                    break;
                default:
                    break;
            }
        }
    };
   private Notification createFtpConnectedNotification() {
        if (VERBOSE) Log.v(TAG, "Creating FTP access CONNECTED");

        Notification notification = new Notification(android.R.drawable.stat_sys_data_bluetooth,
            getString(R.string.ftp_notif_active_session), System.currentTimeMillis());
        notification.setLatestEventInfo(this,  getString(R.string.ftp_notif_active_session),
            getString( R.string.ftp_notif_connected , getRemoteDeviceName()), null);
        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        notification.flags |= Notification.FLAG_ONLY_ALERT_ONCE;
        notification.defaults = Notification.DEFAULT_SOUND;
        return notification;
    }
    private void createFtpNotification(String action) {

        NotificationManager nm = (NotificationManager)
            getSystemService(Context.NOTIFICATION_SERVICE);

        // Create an intent triggered by clicking on the status icon.
        Intent clickIntent = new Intent();
        clickIntent.setClass(this, BluetoothFtpActivity.class);
        clickIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        clickIntent.setAction(action);

        // Create an intent triggered by clicking on the
        // "Clear All Notifications" button
        Intent deleteIntent = new Intent();
        deleteIntent.setClass(this, BluetoothFtpReceiver.class);

       Log.v(TAG,"createFtpNotification: action: "+action);
        Notification notification = null;
        String name = getRemoteDeviceName();
        if (action.equals(ACCESS_REQUEST_ACTION)) {
            deleteIntent.setAction(ACCESS_ALLOWED_ACTION);
            notification = new Notification(android.R.drawable.stat_sys_data_bluetooth,
                getString(R.string.ftp_notif_ticker), System.currentTimeMillis());
            notification.setLatestEventInfo(this, getString(R.string.ftp_notif_ticker),
                    getString(R.string.ftp_notif_message, name), PendingIntent
                            .getActivity(this, 0, clickIntent, 0));
            notification.flags |= Notification.FLAG_AUTO_CANCEL;
            notification.flags |= Notification.FLAG_ONLY_ALERT_ONCE;
            notification.defaults = Notification.DEFAULT_SOUND;
            notification.deleteIntent = PendingIntent.getBroadcast(this, 0, deleteIntent, 0);
            nm.notify(NOTIFICATION_ID_ACCESS, notification);
        } else if (action.equals(AUTH_CHALL_ACTION)) {
            deleteIntent.setAction(AUTH_CANCELLED_ACTION);
            notification = new Notification(android.R.drawable.stat_sys_data_bluetooth,
                getString(R.string.ftp_notif_ticker), System.currentTimeMillis());
            notification.setLatestEventInfo(this, getString(R.string.ftp_notif_title),
                    getString(R.string.ftp_notif_message, name), PendingIntent
                            .getActivity(this, 0, clickIntent, 0));

            notification.flags |= Notification.FLAG_AUTO_CANCEL;
            notification.flags |= Notification.FLAG_ONLY_ALERT_ONCE;
            notification.defaults = Notification.DEFAULT_SOUND;
            notification.deleteIntent = PendingIntent.getBroadcast(this, 0, deleteIntent, 0);
            nm.notify(NOTIFICATION_ID_AUTH, notification);
        }
    }

    private void removeFtpNotification(int id) {
        Context context = getApplicationContext();
        NotificationManager nm = (NotificationManager)context
                .getSystemService(Context.NOTIFICATION_SERVICE);
        nm.cancel(id);
    }

    private void removeAuthChallTimer() {
        if (mSessionStatusHandler != null)
          mSessionStatusHandler.removeMessages(MSG_INTERNAL_AUTH_TIMEOUT);
    }

    public static String getRemoteDeviceName() {
        return sRemoteDeviceName;
    }

    public static class FtpMediaScannerNotifier implements MediaScannerConnectionClient {

        private MediaScannerConnection mConnection;

        private Context mContext;

        private Handler mCallback;

        private int mOp;

        public FtpMediaScannerNotifier(Context context,final String filename,
                                                 final String mimetype,Handler handler,int op) {
            mContext = context;
            mCallback = handler;
            mOp = op;
            if (VERBOSE) Log.v(TAG, "FTP MediaScannerConnection FtpMediaScannerNotifier mFilename ="
                                + filename + " mMimetype = " + mimetype +"operation " + mOp);
            List<String> filenames = new ArrayList<String>();
            List<String> types = new ArrayList<String>();

            filenames.add(filename);
            types.add(mimetype);
            MediaScannerConnection.scanFile(context,filenames.toArray(new String[filenames.size()]),
                                             types.toArray(new String[types.size()]),
                                             this);
        }

        public FtpMediaScannerNotifier(Context context,final String[] filenames,
                                                 final String[] mimetypes,Handler handler,int op) {
            mContext = context;
            mCallback = handler;
            mOp = op;
            if (VERBOSE) Log.v(TAG, "FtpMediaScannerNotifier scan for multiple files " +
                                                         filenames.length +" " +mimetypes.length );
            MediaScannerConnection.scanFile(context,filenames,mimetypes,
                                             this);
        }

        public void onMediaScannerConnected() {
            if (VERBOSE) Log.v(TAG, "FTP MediaScannerConnection onMediaScannerConnected");
        }

        public void onScanCompleted(String path, Uri uri) {
            try {
                if (VERBOSE) {
                    Log.v(TAG, "FTP MediaScannerConnection onScanCompleted");
                    Log.v(TAG, "FTP MediaScannerConnection path is " + path);
                    Log.v(TAG, "FTP MediaScannerConnection Uri is " + uri);
                    Log.v(TAG, "FTP MediaScannerConnection mOp is " + mOp);
                }
                if (uri != null) {
                    Message msg = Message.obtain();
                    msg.setTarget(mCallback);
                    msg.what = FTP_MEDIA_SCANNED;
                    msg.arg1 = mOp;
                    msg.obj = uri;
                    msg.sendToTarget();
                } else {
                    Message msg = Message.obtain();
                    msg.setTarget(mCallback);
                    msg.what = FTP_MEDIA_SCANNED_FAILED;
                    msg.arg1 = mOp;
                    msg.sendToTarget();
                }
            } catch (Exception ex) {
                Log.e(TAG, "FTP !!!MediaScannerConnection exception: " + ex);
            } finally {
                if (VERBOSE) Log.v(TAG, "FTP MediaScannerConnection disconnect");
            }
        }
    };
};
