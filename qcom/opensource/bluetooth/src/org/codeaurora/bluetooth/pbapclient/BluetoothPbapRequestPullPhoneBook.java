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

import com.android.vcard.VCardEntry;
import org.codeaurora.bluetooth.utils.ObexAppParameters;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

import javax.obex.HeaderSet;

final class BluetoothPbapRequestPullPhoneBook extends BluetoothPbapRequest {

    private static final String TAG = "BluetoothPbapRequestPullPhoneBook";

    private static final String TYPE = "x-bt/phonebook";

    private BluetoothPbapVcardList mResponse;

    private int mNewMissedCalls = -1;

    private final byte mFormat;

    public BluetoothPbapRequestPullPhoneBook(String pbName, long filter, byte format,
            int maxListCount, int listStartOffset) {

        if (maxListCount < 0 || maxListCount > 65535) {
            throw new IllegalArgumentException("maxListCount should be [0..65535]");
        }

        if (listStartOffset < 0 || listStartOffset > 65535) {
            throw new IllegalArgumentException("listStartOffset should be [0..65535]");
        }

        mHeaderSet.setHeader(HeaderSet.NAME, pbName);

        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);

        ObexAppParameters oap = new ObexAppParameters();

        /* make sure format is one of allowed values */
        if (format != BluetoothPbapClient.VCARD_TYPE_21
                && format != BluetoothPbapClient.VCARD_TYPE_30) {
            format = BluetoothPbapClient.VCARD_TYPE_21;
        }

        if (filter != 0) {
            oap.add(OAP_TAGID_FILTER, filter);
        }

        oap.add(OAP_TAGID_FORMAT, format);

        /*
         * maxListCount is a special case which is handled in
         * BluetoothPbapRequestPullPhoneBookSize
         */
        if (maxListCount > 0) {
            oap.add(OAP_TAGID_MAX_LIST_COUNT, (short) maxListCount);
        } else {
            oap.add(OAP_TAGID_MAX_LIST_COUNT, (short) 65535);
        }

        if (listStartOffset > 0) {
            oap.add(OAP_TAGID_LIST_START_OFFSET, (short) listStartOffset);
        }

        oap.addToHeaderSet(mHeaderSet);

        mFormat = format;
    }

    @Override
    protected void readResponse(InputStream stream) throws IOException {
        Log.v(TAG, "readResponse");

        mResponse = new BluetoothPbapVcardList(stream, mFormat);
    }

    @Override
    protected void readResponseHeaders(HeaderSet headerset) {
        Log.v(TAG, "readResponse");

        ObexAppParameters oap = ObexAppParameters.fromHeaderSet(headerset);

        if (oap.exists(OAP_TAGID_NEW_MISSED_CALLS)) {
            mNewMissedCalls = oap.getByte(OAP_TAGID_NEW_MISSED_CALLS);
        }
    }

    public ArrayList<VCardEntry> getList() {
        return mResponse.getList();
    }

    public int getNewMissedCalls() {
        return mNewMissedCalls;
    }
}
