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
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.DataInputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.util.HashMap;

/**
 * Object representation of event report received by MNS
 * <p>
 * This object will be received in {@link BluetoothMasClient#EVENT_EVENT_REPORT}
 * callback message.
 */
public class BluetoothMapEventReport {

    private final static String TAG = "BluetoothMapEventReport";

    public enum Type {
        NEW_MESSAGE("NewMessage"), DELIVERY_SUCCESS("DeliverySuccess"),
        SENDING_SUCCESS("SendingSuccess"), DELIVERY_FAILURE("DeliveryFailure"),
        SENDING_FAILURE("SendingFailure"), MEMORY_FULL("MemoryFull"),
        MEMORY_AVAILABLE("MemoryAvailable"), MESSAGE_DELETED("MessageDeleted"),
        MESSAGE_SHIFT("MessageShift");

        private final String mSpecName;

        private Type(String specName) {
            mSpecName = specName;
        }

        @Override
        public String toString() {
            return mSpecName;
        }
    }

    private final Type mType;

    private final String mHandle;

    private final String mFolder;

    private final String mOldFolder;

    private final BluetoothMapBmessage.Type mMsgType;

    private BluetoothMapEventReport(HashMap<String, String> attrs) throws IllegalArgumentException {
        mType = parseType(attrs.get("type"));

        if (mType != Type.MEMORY_FULL && mType != Type.MEMORY_AVAILABLE) {
            String handle = attrs.get("handle");
            try {
                /* just to validate */
                new BigInteger(attrs.get("handle"), 16);

                mHandle = attrs.get("handle");
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("Invalid value for handle:" + handle);
            }
        } else {
            mHandle = null;
        }

        mFolder = attrs.get("folder");

        mOldFolder = attrs.get("old_folder");

        if (mType != Type.MEMORY_FULL && mType != Type.MEMORY_AVAILABLE) {
            String s = attrs.get("msg_type");

            if ("".equals(s)) {
                // Some phones (e.g. SGS3 for MessageDeleted) send empty
                // msg_type, in such case leave it as null rather than throw
                // parse exception
                mMsgType = null;
            } else {
                mMsgType = parseMsgType(s);
            }
        } else {
            mMsgType = null;
        }
    }

    private Type parseType(String type) throws IllegalArgumentException {
        for (Type t : Type.values()) {
            if (t.toString().equals(type)) {
                return t;
            }
        }

        throw new IllegalArgumentException("Invalid value for type: " + type);
    }

    private BluetoothMapBmessage.Type parseMsgType(String msgType) throws IllegalArgumentException {
        for (BluetoothMapBmessage.Type t : BluetoothMapBmessage.Type.values()) {
            if (t.name().equals(msgType)) {
                return t;
            }
        }

        throw new IllegalArgumentException("Invalid value for msg_type: " + msgType);
    }

    /**
     * @return {@link BluetoothMapEventReport.Type} object corresponding to
     *         <code>type</code> application parameter in MAP specification
     */
    public Type getType() {
        return mType;
    }

    /**
     * @return value corresponding to <code>handle</code> parameter in MAP
     *         specification
     */
    public String getHandle() {
        return mHandle;
    }

    /**
     * @return value corresponding to <code>folder</code> parameter in MAP
     *         specification
     */
    public String getFolder() {
        return mFolder;
    }

    /**
     * @return value corresponding to <code>old_folder</code> parameter in MAP
     *         specification
     */
    public String getOldFolder() {
        return mOldFolder;
    }

    /**
     * @return {@link BluetoothMapBmessage.Type} object corresponding to
     *         <code>msg_type</code> application parameter in MAP specification
     */
    public BluetoothMapBmessage.Type getMsgType() {
        return mMsgType;
    }

    @Override
    public String toString() {
        JSONObject json = new JSONObject();

        try {
            json.put("type", mType);
            json.put("handle", mHandle);
            json.put("folder", mFolder);
            json.put("old_folder", mOldFolder);
            json.put("msg_type", mMsgType);
        } catch (JSONException e) {
            // do nothing
        }

        return json.toString();
    }

    static BluetoothMapEventReport fromStream(DataInputStream in) {
        BluetoothMapEventReport ev = null;

        try {
            XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
            xpp.setInput(in, "utf-8");

            int event = xpp.getEventType();
            while (event != XmlPullParser.END_DOCUMENT) {
                switch (event) {
                    case XmlPullParser.START_TAG:
                        if (xpp.getName().equals("event")) {
                            HashMap<String, String> attrs = new HashMap<String, String>();

                            for (int i = 0; i < xpp.getAttributeCount(); i++) {
                                attrs.put(xpp.getAttributeName(i), xpp.getAttributeValue(i));
                            }

                            ev = new BluetoothMapEventReport(attrs);

                            // return immediately, only one event should be here
                            return ev;
                        }
                        break;
                }

                event = xpp.next();
            }

        } catch (XmlPullParserException e) {
            Log.e(TAG, "XML parser error when parsing XML", e);
        } catch (IOException e) {
            Log.e(TAG, "I/O error when parsing XML", e);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Invalid event received", e);
        }

        return ev;
    }
}
