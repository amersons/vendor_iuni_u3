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

import com.android.vcard.VCardEntry;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

/**
 * Object representation of message in bMessage format
 * <p>
 * This object will be received in {@link BluetoothMasClient#EVENT_GET_MESSAGE}
 * callback message.
 */
public class BluetoothMapBmessage {

    String mBmsgVersion;
    Status mBmsgStatus;
    Type mBmsgType;
    String mBmsgFolder;

    String mBbodyEncoding;
    String mBbodyCharset;
    String mBbodyLanguage;
    int mBbodyLength;

    String mMessage;

    ArrayList<VCardEntry> mOriginators;
    ArrayList<VCardEntry> mRecipients;

    public enum Status {
        READ, UNREAD
    }

    public enum Type {
        EMAIL, SMS_GSM, SMS_CDMA, MMS
    }

    /**
     * Constructs empty message object
     */
    public BluetoothMapBmessage() {
        mOriginators = new ArrayList<VCardEntry>();
        mRecipients = new ArrayList<VCardEntry>();
    }

    public VCardEntry getOriginator() {
        if (mOriginators.size() > 0) {
            return mOriginators.get(0);
        } else {
            return null;
        }
    }

    public ArrayList<VCardEntry> getOriginators() {
        return mOriginators;
    }

    public BluetoothMapBmessage addOriginator(VCardEntry vcard) {
        mOriginators.add(vcard);
        return this;
    }

    public ArrayList<VCardEntry> getRecipients() {
        return mRecipients;
    }

    public BluetoothMapBmessage addRecipient(VCardEntry vcard) {
        mRecipients.add(vcard);
        return this;
    }

    public Status getStatus() {
        return mBmsgStatus;
    }

    public BluetoothMapBmessage setStatus(Status status) {
        mBmsgStatus = status;
        return this;
    }

    public Type getType() {
        return mBmsgType;
    }

    public BluetoothMapBmessage setType(Type type) {
        mBmsgType = type;
        return this;
    }

    public String getFolder() {
        return mBmsgFolder;
    }

    public BluetoothMapBmessage setFolder(String folder) {
        mBmsgFolder = folder;
        return this;
    }

    public String getEncoding() {
        return mBbodyEncoding;
    }

    public BluetoothMapBmessage setEncoding(String encoding) {
        mBbodyEncoding = encoding;
        return this;
    }

    public String getCharset() {
        return mBbodyCharset;
    }

    public BluetoothMapBmessage setCharset(String charset) {
        mBbodyCharset = charset;
        return this;
    }

    public String getLanguage() {
        return mBbodyLanguage;
    }

    public BluetoothMapBmessage setLanguage(String language) {
        mBbodyLanguage = language;
        return this;
    }

    public String getBodyContent() {
        return mMessage;
    }

    public BluetoothMapBmessage setBodyContent(String body) {
        mMessage = body;
        return this;
    }

    @Override
    public String toString() {
        JSONObject json = new JSONObject();

        try {
            json.put("status", mBmsgStatus);
            json.put("type", mBmsgType);
            json.put("folder", mBmsgFolder);
            json.put("message", mMessage);
        } catch (JSONException e) {
            // do nothing
        }

        return json.toString();
    }
}
