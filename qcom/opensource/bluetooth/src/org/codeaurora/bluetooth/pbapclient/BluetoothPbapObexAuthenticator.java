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


package org.codeaurora.bluetooth.pbapclient;

import android.os.Handler;
import android.util.Log;

import javax.obex.Authenticator;
import javax.obex.PasswordAuthentication;

class BluetoothPbapObexAuthenticator implements Authenticator {

    private final static String TAG = "BluetoothPbapObexAuthenticator";

    private String mSessionKey;

    private boolean mReplied;

    private final Handler mCallback;

    public BluetoothPbapObexAuthenticator(Handler callback) {
        mCallback = callback;
    }

    public synchronized void setReply(String key) {
        Log.d(TAG, "setReply key=" + key);

        mSessionKey = key;
        mReplied = true;

        notify();
    }

    @Override
    public PasswordAuthentication onAuthenticationChallenge(String description,
            boolean isUserIdRequired, boolean isFullAccess) {
        PasswordAuthentication pa = null;

        mReplied = false;

        Log.d(TAG, "onAuthenticationChallenge: sending request");
        mCallback.obtainMessage(BluetoothPbapObexSession.OBEX_SESSION_AUTHENTICATION_REQUEST)
                .sendToTarget();

        synchronized (this) {
            while (!mReplied) {
                try {
                    Log.v(TAG, "onAuthenticationChallenge: waiting for response");
                    this.wait();
                } catch (InterruptedException e) {
                    Log.e(TAG, "Interrupted while waiting for challenge response");
                }
            }
        }

        if (mSessionKey != null && mSessionKey.length() != 0) {
            Log.v(TAG, "onAuthenticationChallenge: mSessionKey=" + mSessionKey);
            pa = new PasswordAuthentication(null, mSessionKey.getBytes());
        } else {
            Log.v(TAG, "onAuthenticationChallenge: mSessionKey is empty, timeout/cancel occured");
        }

        return pa;
    }

    @Override
    public byte[] onAuthenticationResponse(byte[] userName) {
        /* required only in case PCE challenges PSE which we don't do now */
        return null;
    }

}
