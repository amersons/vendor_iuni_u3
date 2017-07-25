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
import android.util.Log;

import org.codeaurora.bluetooth.utils.ObexAppParameters;

import java.io.IOException;
import java.util.Arrays;

import javax.obex.HeaderSet;
import javax.obex.Operation;
import javax.obex.ResponseCodes;
import javax.obex.ServerRequestHandler;

class BluetoothMnsObexServer extends ServerRequestHandler {

    private final static String TAG = "BluetoothMnsObexServer";

    private static final byte[] MNS_TARGET = new byte[] {
            (byte) 0xbb, 0x58, 0x2b, 0x41, 0x42, 0x0c, 0x11, (byte) 0xdb, (byte) 0xb0, (byte) 0xde,
            0x08, 0x00, 0x20, 0x0c, (byte) 0x9a, 0x66
    };

    private final static String TYPE = "x-bt/MAP-event-report";

    private final Handler mCallback;

    public BluetoothMnsObexServer(Handler callback) {
        super();

        mCallback = callback;
    }

    @Override
    public int onConnect(final HeaderSet request, HeaderSet reply) {
        Log.v(TAG, "onConnect");

        try {
            byte[] uuid = (byte[]) request.getHeader(HeaderSet.TARGET);

            if (!Arrays.equals(uuid, MNS_TARGET)) {
                return ResponseCodes.OBEX_HTTP_NOT_ACCEPTABLE;
            }

        } catch (IOException e) {
            // this should never happen since getHeader won't throw exception it
            // declares to throw
            return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }

        reply.setHeader(HeaderSet.WHO, MNS_TARGET);
        return ResponseCodes.OBEX_HTTP_OK;
    }

    @Override
    public void onDisconnect(final HeaderSet request, HeaderSet reply) {
        Log.v(TAG, "onDisconnect");
    }

    @Override
    public int onGet(final Operation op) {
        Log.v(TAG, "onGet");

        return ResponseCodes.OBEX_HTTP_BAD_REQUEST;
    }

    @Override
    public int onPut(final Operation op) {
        Log.v(TAG, "onPut");

        try {
            HeaderSet headerset;
            headerset = op.getReceivedHeader();

            String type = (String) headerset.getHeader(HeaderSet.TYPE);
            ObexAppParameters oap = ObexAppParameters.fromHeaderSet(headerset);

            if (!TYPE.equals(type) || !oap.exists(BluetoothMasRequest.OAP_TAGID_MAS_INSTANCE_ID)) {
                return ResponseCodes.OBEX_HTTP_BAD_REQUEST;
            }

            Byte inst = oap.getByte(BluetoothMasRequest.OAP_TAGID_MAS_INSTANCE_ID);

            BluetoothMapEventReport ev = BluetoothMapEventReport.fromStream(op
                    .openDataInputStream());

            op.close();

            mCallback.obtainMessage(BluetoothMnsService.MSG_EVENT, inst, 0, ev).sendToTarget();
        } catch (IOException e) {
            Log.e(TAG, "I/O exception when handling PUT request", e);
            return ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }

        return ResponseCodes.OBEX_HTTP_OK;
    }

    @Override
    public int onAbort(final HeaderSet request, HeaderSet reply) {
        Log.v(TAG, "onAbort");

        return ResponseCodes.OBEX_HTTP_NOT_IMPLEMENTED;
    }

    @Override
    public int onSetPath(final HeaderSet request, HeaderSet reply,
            final boolean backup, final boolean create) {
        Log.v(TAG, "onSetPath");

        return ResponseCodes.OBEX_HTTP_BAD_REQUEST;
    }

    @Override
    public void onClose() {
        Log.v(TAG, "onClose");

        // TODO: call session handler so it can disconnect
    }
}
