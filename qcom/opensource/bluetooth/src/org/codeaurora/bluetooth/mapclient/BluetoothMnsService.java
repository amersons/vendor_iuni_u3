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



package org.codeaurora.bluetooth.mapclient;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.util.Log;
import android.util.SparseArray;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.lang.ref.WeakReference;

import javax.obex.ServerSession;

class BluetoothMnsService {

    private static final String TAG = "BluetoothMnsService";

    private static final ParcelUuid MAP_MNS =
            ParcelUuid.fromString("00001133-0000-1000-8000-00805F9B34FB");

    static final int MSG_EVENT = 1;

    /* for BluetoothMasClient */
    static final int EVENT_REPORT = 1001;

    /* these are shared across instances */
    static private SparseArray<Handler> mCallbacks = null;
    static private SocketAcceptThread mAcceptThread = null;
    static private Handler mSessionHandler = null;
    static private BluetoothServerSocket mServerSocket = null;

    private static class SessionHandler extends Handler {

        private final WeakReference<BluetoothMnsService> mService;

        SessionHandler(BluetoothMnsService service) {
            mService = new WeakReference<BluetoothMnsService>(service);
        }

        @Override
        public void handleMessage(Message msg) {
            Log.d(TAG, "Handler: msg: " + msg.what);

            switch (msg.what) {
                case MSG_EVENT:
                    int instanceId = msg.arg1;

                    synchronized (mCallbacks) {
                        Handler cb = mCallbacks.get(instanceId);

                        if (cb != null) {
                            BluetoothMapEventReport ev = (BluetoothMapEventReport) msg.obj;
                            cb.obtainMessage(EVENT_REPORT, ev).sendToTarget();
                        } else {
                            Log.w(TAG, "Got event for instance which is not registered: "
                                    + instanceId);
                        }
                    }
                    break;
            }
        }
    }

    private static class SocketAcceptThread extends Thread {

        private boolean mInterrupted = false;

        @Override
        public void run() {

            if (mServerSocket != null) {
                Log.w(TAG, "Socket already created, exiting");
                return;
            }

            try {
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                mServerSocket = adapter.listenUsingEncryptedRfcommWithServiceRecord(
                        "MAP Message Notification Service", MAP_MNS.getUuid());
            } catch (IOException e) {
                mInterrupted = true;
                Log.e(TAG, "I/O exception when trying to create server socket", e);
            }

            while (!mInterrupted) {
                try {
                    Log.v(TAG, "waiting to accept connection...");

                    BluetoothSocket sock = mServerSocket.accept();

                    Log.v(TAG, "new incoming connection from "
                            + sock.getRemoteDevice().getName());

                    // session will live until closed by remote
                    BluetoothMnsObexServer srv = new BluetoothMnsObexServer(mSessionHandler);
                    BluetoothMapRfcommTransport transport = new BluetoothMapRfcommTransport(
                            sock);
                    new ServerSession(transport, srv, null);
                } catch (IOException ex) {
                    Log.v(TAG, "I/O exception when waiting to accept (aborted?)");
                    mInterrupted = true;
                }
            }

            if (mServerSocket != null) {
                try {
                    mServerSocket.close();
                } catch (IOException e) {
                    // do nothing
                }

                mServerSocket = null;
            }
        }
    }

    BluetoothMnsService() {
        Log.v(TAG, "BluetoothMnsService()");

        if (mCallbacks == null) {
            Log.v(TAG, "BluetoothMnsService(): allocating callbacks");
            mCallbacks = new SparseArray<Handler>();
        }

        if (mSessionHandler == null) {
            Log.v(TAG, "BluetoothMnsService(): allocating session handler");
            mSessionHandler = new SessionHandler(this);
        }
    }

    public void registerCallback(int instanceId, Handler callback) {
        Log.v(TAG, "registerCallback()");

        synchronized (mCallbacks) {
            mCallbacks.put(instanceId, callback);

            if (mAcceptThread == null) {
                Log.v(TAG, "registerCallback(): starting MNS server");
                mAcceptThread = new SocketAcceptThread();
                mAcceptThread.setName("BluetoothMnsAcceptThread");
                mAcceptThread.start();
            }
        }
    }

    public void unregisterCallback(int instanceId) {
        Log.v(TAG, "unregisterCallback()");

        synchronized (mCallbacks) {
            mCallbacks.remove(instanceId);

            if (mCallbacks.size() == 0) {
                Log.v(TAG, "unregisterCallback(): shutting down MNS server");

                if (mServerSocket != null) {
                    try {
                        mServerSocket.close();
                    } catch (IOException e) {
                    }

                    mServerSocket = null;
                }

                mAcceptThread.interrupt();

                try {
                    mAcceptThread.join(5000);
                } catch (InterruptedException e) {
                }

                mAcceptThread = null;
            }
        }
    }
}
