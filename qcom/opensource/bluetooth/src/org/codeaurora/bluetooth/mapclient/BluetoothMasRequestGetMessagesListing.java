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

import org.codeaurora.bluetooth.mapclient.BluetoothMasClient.MessagesFilter;
import org.codeaurora.bluetooth.utils.ObexAppParameters;
import org.codeaurora.bluetooth.utils.ObexTime;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Date;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;

final class BluetoothMasRequestGetMessagesListing extends BluetoothMasRequest {

    private static final String TYPE = "x-bt/MAP-msg-listing";

    private BluetoothMapMessagesListing mResponse = null;

    private boolean mNewMessage = false;

    private Date mServerTime = null;

    public BluetoothMasRequestGetMessagesListing(String folderName, int parameters,
            BluetoothMasClient.MessagesFilter filter, int subjectLength, int maxListCount,
            int listStartOffset) {

        if (subjectLength < 0 || subjectLength > 255) {
            throw new IllegalArgumentException("subjectLength should be [0..255]");
        }

        if (maxListCount < 0 || maxListCount > 65535) {
            throw new IllegalArgumentException("maxListCount should be [0..65535]");
        }

        if (listStartOffset < 0 || listStartOffset > 65535) {
            throw new IllegalArgumentException("listStartOffset should be [0..65535]");
        }

        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);

        if (folderName == null) {
            mHeaderSet.setHeader(HeaderSet.NAME, "");
        } else {
            mHeaderSet.setHeader(HeaderSet.NAME, folderName);
        }

        ObexAppParameters oap = new ObexAppParameters();

        if (filter != null) {
            if (filter.messageType != MessagesFilter.MESSAGE_TYPE_ALL) {
                oap.add(OAP_TAGID_FILTER_MESSAGE_TYPE, filter.messageType);
            }

            if (filter.periodBegin != null) {
                oap.add(OAP_TAGID_FILTER_PERIOD_BEGIN, filter.periodBegin);
            }

            if (filter.periodEnd != null) {
                oap.add(OAP_TAGID_FILTER_PERIOD_END, filter.periodEnd);
            }

            if (filter.readStatus != MessagesFilter.READ_STATUS_ANY) {
                oap.add(OAP_TAGID_FILTER_READ_STATUS, filter.readStatus);
            }

            if (filter.recipient != null) {
                oap.add(OAP_TAGID_FILTER_RECIPIENT, filter.recipient);
            }

            if (filter.originator != null) {
                oap.add(OAP_TAGID_FILTER_ORIGINATOR, filter.originator);
            }

            if (filter.priority != MessagesFilter.PRIORITY_ANY) {
                oap.add(OAP_TAGID_FILTER_PRIORITY, filter.priority);
            }
        }

        if (subjectLength != 0) {
            oap.add(OAP_TAGID_SUBJECT_LENGTH, (byte) subjectLength);
        }

        if (maxListCount != 0) {
            oap.add(OAP_TAGID_MAX_LIST_COUNT, (short) maxListCount);
        }

        if (listStartOffset != 0) {
            oap.add(OAP_TAGID_START_OFFSET, (short) listStartOffset);
        }

        oap.addToHeaderSet(mHeaderSet);
    }

    @Override
    protected void readResponse(InputStream stream) {
        mResponse = new BluetoothMapMessagesListing(stream);
    }

    @Override
    protected void readResponseHeaders(HeaderSet headerset) {
        ObexAppParameters oap = ObexAppParameters.fromHeaderSet(headerset);

        mNewMessage = ((oap.getByte(OAP_TAGID_NEW_MESSAGE) & 0x01) == 1);

        if (oap.exists(OAP_TAGID_MSE_TIME)) {
            String mseTime = oap.getString(OAP_TAGID_MSE_TIME);

            mServerTime = (new ObexTime(mseTime)).getTime();
        }
    }

    public ArrayList<BluetoothMapMessage> getList() {
        if (mResponse == null) {
            return null;
        }

        return mResponse.getList();
    }

    public boolean getNewMessageStatus() {
        return mNewMessage;
    }

    public Date getMseTime() {
        return mServerTime;
    }

    @Override
    public void execute(ClientSession session) throws IOException {
        executeGet(session);
    }
}
