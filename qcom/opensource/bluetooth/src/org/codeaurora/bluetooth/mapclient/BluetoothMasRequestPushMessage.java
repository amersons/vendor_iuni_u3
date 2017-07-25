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

import java.io.IOException;
import java.math.BigInteger;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ResponseCodes;

import org.codeaurora.bluetooth.mapclient.BluetoothMasClient.CharsetType;
import org.codeaurora.bluetooth.utils.ObexAppParameters;

final class BluetoothMasRequestPushMessage extends BluetoothMasRequest {

    private static final String TYPE = "x-bt/message";
    private String mMsg;
    private String mMsgHandle;

    private BluetoothMasRequestPushMessage(String folder) {
        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);
        if (folder == null) {
            folder = "";
        }
        mHeaderSet.setHeader(HeaderSet.NAME, folder);
    }

    public BluetoothMasRequestPushMessage(String folder, String msg, CharsetType charset,
            boolean transparent, boolean retry) {
        this(folder);
        mMsg = msg;
        ObexAppParameters oap = new ObexAppParameters();
        oap.add(OAP_TAGID_TRANSPARENT, transparent ? TRANSPARENT_ON : TRANSPARENT_OFF);
        oap.add(OAP_TAGID_RETRY, retry ? RETRY_ON : RETRY_OFF);
        oap.add(OAP_TAGID_CHARSET, charset == CharsetType.NATIVE ? CHARSET_NATIVE : CHARSET_UTF8);
        oap.addToHeaderSet(mHeaderSet);
    }

    @Override
    protected void readResponseHeaders(HeaderSet headerset) {
        try {
            String handle = (String) headerset.getHeader(HeaderSet.NAME);
            if (handle != null) {
                /* just to validate */
                new BigInteger(handle, 16);

                mMsgHandle = handle;
            }
        } catch (NumberFormatException e) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        } catch (IOException e) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
        }
    }

    public String getMsgHandle() {
        return mMsgHandle;
    }

    @Override
    public void execute(ClientSession session) throws IOException {
        executePut(session, mMsg.getBytes());
    }
}
