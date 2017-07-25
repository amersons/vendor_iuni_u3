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

import android.os.Handler;
import android.os.Process;
import android.util.Log;

import java.io.IOException;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ObexTransport;
import javax.obex.ResponseCodes;

class BluetoothMasObexClientSession {
    private static final String TAG = "BluetoothMasObexClientSession";

    private static final byte[] MAS_TARGET = new byte[] {
            (byte) 0xbb, 0x58, 0x2b, 0x40, 0x42, 0x0c, 0x11, (byte) 0xdb, (byte) 0xb0, (byte) 0xde,
            0x08, 0x00, 0x20, 0x0c, (byte) 0x9a, 0x66
    };

    static final int MSG_OBEX_CONNECTED = 100;
    static final int MSG_OBEX_DISCONNECTED = 101;
    static final int MSG_REQUEST_COMPLETED = 102;

    private final ObexTransport mTransport;

    private final Handler mSessionHandler;

    private ClientThread mClientThread;

    private volatile boolean mInterrupted;

    private class ClientThread extends Thread {
        private final ObexTransport mTransport;

        private ClientSession mSession;

        private BluetoothMasRequest mRequest;

        private boolean mConnected;

        public ClientThread(ObexTransport transport) {
            super("MAS ClientThread");

            mTransport = transport;
            mConnected = false;
        }

        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);

            connect();

            if (mConnected) {
                mSessionHandler.obtainMessage(MSG_OBEX_CONNECTED).sendToTarget();
            } else {
                mSessionHandler.obtainMessage(MSG_OBEX_DISCONNECTED).sendToTarget();
                return;
            }

            while (!mInterrupted) {
                synchronized (this) {
                    if (mRequest == null) {
                        try {
                            this.wait();
                        } catch (InterruptedException e) {
                            mInterrupted = true;
                        }
                    }
                }

                if (!mInterrupted && mRequest != null) {
                    try {
                        mRequest.execute(mSession);
                    } catch (IOException e) {
                        // this will "disconnect" to cleanup
                        mInterrupted = true;
                    }

                    BluetoothMasRequest oldReq = mRequest;
                    mRequest = null;

                    mSessionHandler.obtainMessage(MSG_REQUEST_COMPLETED, oldReq).sendToTarget();
                }
            }

            disconnect();

            mSessionHandler.obtainMessage(MSG_OBEX_DISCONNECTED).sendToTarget();
        }

        private void connect() {
            try {
                mSession = new ClientSession(mTransport);

                HeaderSet headerset = new HeaderSet();
                headerset.setHeader(HeaderSet.TARGET, MAS_TARGET);

                headerset = mSession.connect(headerset);

                if (headerset.getResponseCode() == ResponseCodes.OBEX_HTTP_OK) {
                    mConnected = true;
                } else {
                    disconnect();
                }
            } catch (IOException e) {
            }
        }

        private void disconnect() {
            try {
                mSession.disconnect(null);
            } catch (IOException e) {
            }

            try {
                mSession.close();
            } catch (IOException e) {
            }

            mConnected = false;
        }

        public synchronized boolean schedule(BluetoothMasRequest request) {
            if (mRequest != null) {
                return false;
            }

            mRequest = request;
            notify();

            return true;
        }
    }

    public BluetoothMasObexClientSession(ObexTransport transport, Handler handler) {
        mTransport = transport;
        mSessionHandler = handler;
    }

    public void start() {
        if (mClientThread == null) {
            mClientThread = new ClientThread(mTransport);
            mClientThread.start();
        }

    }

    public void stop() {
        if (mClientThread != null) {
            mClientThread.interrupt();

            (new Thread() {
                @Override
                public void run() {
                    try {
                        mClientThread.join();
                        mClientThread = null;
                    } catch (InterruptedException e) {
                        Log.w(TAG, "Interrupted while waiting for thread to join");
                    }
                }
            }).run();
        }
    }

    public boolean makeRequest(BluetoothMasRequest request) {
        if (mClientThread == null) {
            return false;
        }

        return mClientThread.schedule(request);
    }
}
