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
import java.io.InputStream;

import javax.obex.ClientOperation;
import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ResponseCodes;

abstract class BluetoothPbapRequest {

    private static final String TAG = "BluetoothPbapRequest";

    protected static final byte OAP_TAGID_ORDER = 0x01;
    protected static final byte OAP_TAGID_SEARCH_VALUE = 0x02;
    protected static final byte OAP_TAGID_SEARCH_ATTRIBUTE = 0x03;
    protected static final byte OAP_TAGID_MAX_LIST_COUNT = 0x04;
    protected static final byte OAP_TAGID_LIST_START_OFFSET = 0x05;
    protected static final byte OAP_TAGID_FILTER = 0x06;
    protected static final byte OAP_TAGID_FORMAT = 0x07;
    protected static final byte OAP_TAGID_PHONEBOOK_SIZE = 0x08;
    protected static final byte OAP_TAGID_NEW_MISSED_CALLS = 0x09;

    protected HeaderSet mHeaderSet;

    protected int mResponseCode;

    private boolean mAborted = false;

    private ClientOperation mOp = null;

    public BluetoothPbapRequest() {
        mHeaderSet = new HeaderSet();
    }

    final public boolean isSuccess() {
        return (mResponseCode == ResponseCodes.OBEX_HTTP_OK);
    }

    public void execute(ClientSession session) throws IOException {
        Log.v(TAG, "execute");

        /* in case request is aborted before can be executed */
        if (mAborted) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
            return;
        }

        try {
            mOp = (ClientOperation) session.get(mHeaderSet);

            /* make sure final flag for GET is used (PBAP spec 6.2.2) */
            mOp.setGetFinalFlag(true);

            /*
             * this will trigger ClientOperation to use non-buffered stream so
             * we can abort operation
             */
            mOp.continueOperation(true, false);

            readResponseHeaders(mOp.getReceivedHeader());

            InputStream is = mOp.openInputStream();
            readResponse(is);
            is.close();

            mOp.close();

            mResponseCode = mOp.getResponseCode();

            Log.d(TAG, "mResponseCode=" + mResponseCode);

            checkResponseCode(mResponseCode);
        } catch (IOException e) {
            Log.e(TAG, "IOException occured when processing request", e);
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;

            throw e;
        }
    }

    public void abort() {
        mAborted = true;

        if (mOp != null) {
            try {
                mOp.abort();
            } catch (IOException e) {
                Log.e(TAG, "Exception occured when trying to abort", e);
            }
        }
    }

    protected void readResponse(InputStream stream) throws IOException {
        Log.v(TAG, "readResponse");

        /* nothing here by default */
    }

    protected void readResponseHeaders(HeaderSet headerset) {
        Log.v(TAG, "readResponseHeaders");

        /* nothing here by dafault */
    }

    protected void checkResponseCode(int responseCode) throws IOException {
        Log.v(TAG, "checkResponseCode");

        /* nothing here by dafault */
    }
}
