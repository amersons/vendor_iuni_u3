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

import android.util.Log;

import java.io.IOException;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ResponseCodes;

final class BluetoothPbapRequestSetPath extends BluetoothPbapRequest {

    private final static String TAG = "BluetoothPbapRequestSetPath";

    private enum SetPathDir {
        ROOT, UP, DOWN
    };

    private SetPathDir mDir;

    public BluetoothPbapRequestSetPath(String name) {
        mDir = SetPathDir.DOWN;
        mHeaderSet.setHeader(HeaderSet.NAME, name);
    }

    public BluetoothPbapRequestSetPath(boolean goUp) {
        mHeaderSet.setEmptyNameHeader();
        if (goUp) {
            mDir = SetPathDir.UP;
        } else {
            mDir = SetPathDir.ROOT;
        }
    }

    @Override
    public void execute(ClientSession session) {
        Log.v(TAG, "execute");

        HeaderSet hs = null;

        try {
            switch (mDir) {
                case ROOT:
                case DOWN:
                    hs = session.setPath(mHeaderSet, false, false);
                    break;
                case UP:
                    hs = session.setPath(mHeaderSet, true, false);
                    break;
            }

            mResponseCode = hs.getResponseCode();
        } catch (IOException e) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }
    }
}
