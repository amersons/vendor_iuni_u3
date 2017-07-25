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
import org.codeaurora.bluetooth.utils.ObexTime;

import org.json.JSONException;
import org.json.JSONObject;

import java.math.BigInteger;
import java.util.Date;
import java.util.HashMap;

/**
 * Object representation of message received in messages listing
 * <p>
 * This object will be received in
 * {@link BluetoothMasClient#EVENT_GET_MESSAGES_LISTING} callback message.
 */
public class BluetoothMapMessage {

    private final String mHandle;

    private final String mSubject;

    private final Date mDateTime;

    private final String mSenderName;

    private final String mSenderAddressing;

    private final String mReplytoAddressing;

    private final String mRecipientName;

    private final String mRecipientAddressing;

    private final Type mType;

    private final int mSize;

    private final boolean mText;

    private final ReceptionStatus mReceptionStatus;

    private final int mAttachmentSize;

    private final boolean mPriority;

    private final boolean mRead;

    private final boolean mSent;

    private final boolean mProtected;

    public enum Type {
        UNKNOWN, EMAIL, SMS_GSM, SMS_CDMA, MMS
    };

    public enum ReceptionStatus {
        UNKNOWN, COMPLETE, FRACTIONED, NOTIFICATION
    }

    BluetoothMapMessage(HashMap<String, String> attrs) throws IllegalArgumentException {
        int size;

        try {
            /* just to validate */
            new BigInteger(attrs.get("handle"), 16);

            mHandle = attrs.get("handle");
        } catch (NumberFormatException e) {
            /*
             * handle MUST have proper value, if it does not then throw
             * something here
             */
            throw new IllegalArgumentException(e);
        }

        mSubject = attrs.get("subject");

        mDateTime = (new ObexTime(attrs.get("datetime"))).getTime();

        mSenderName = attrs.get("sender_name");

        mSenderAddressing = attrs.get("sender_addressing");

        mReplytoAddressing = attrs.get("replyto_addressing");

        mRecipientName = attrs.get("recipient_name");

        mRecipientAddressing = attrs.get("recipient_addressing");

        mType = strToType(attrs.get("type"));

        try {
            size = Integer.parseInt(attrs.get("size"));
        } catch (NumberFormatException e) {
            size = 0;
        }

        mSize = size;

        mText = yesnoToBoolean(attrs.get("text"));

        mReceptionStatus = strToReceptionStatus(attrs.get("reception_status"));

        try {
            size = Integer.parseInt(attrs.get("attachment_size"));
        } catch (NumberFormatException e) {
            size = 0;
        }

        mAttachmentSize = size;

        mPriority = yesnoToBoolean(attrs.get("priority"));

        mRead = yesnoToBoolean(attrs.get("read"));

        mSent = yesnoToBoolean(attrs.get("sent"));

        mProtected = yesnoToBoolean(attrs.get("protected"));
    }

    private boolean yesnoToBoolean(String yesno) {
        return "yes".equals(yesno);
    }

    private Type strToType(String s) {
        if ("EMAIL".equals(s)) {
            return Type.EMAIL;
        } else if ("SMS_GSM".equals(s)) {
            return Type.SMS_GSM;
        } else if ("SMS_CDMA".equals(s)) {
            return Type.SMS_CDMA;
        } else if ("MMS".equals(s)) {
            return Type.MMS;
        }

        return Type.UNKNOWN;
    }

    private ReceptionStatus strToReceptionStatus(String s) {
        if ("complete".equals(s)) {
            return ReceptionStatus.COMPLETE;
        } else if ("fractioned".equals(s)) {
            return ReceptionStatus.FRACTIONED;
        } else if ("notification".equals(s)) {
            return ReceptionStatus.NOTIFICATION;
        }

        return ReceptionStatus.UNKNOWN;
    }

    @Override
    public String toString() {
        JSONObject json = new JSONObject();

        try {
            json.put("handle", mHandle);
            json.put("subject", mSubject);
            json.put("datetime", mDateTime);
            json.put("sender_name", mSenderName);
            json.put("sender_addressing", mSenderAddressing);
            json.put("replyto_addressing", mReplytoAddressing);
            json.put("recipient_name", mRecipientName);
            json.put("recipient_addressing", mRecipientAddressing);
            json.put("type", mType);
            json.put("size", mSize);
            json.put("text", mText);
            json.put("reception_status", mReceptionStatus);
            json.put("attachment_size", mAttachmentSize);
            json.put("priority", mPriority);
            json.put("read", mRead);
            json.put("sent", mSent);
            json.put("protected", mProtected);
        } catch (JSONException e) {
            // do nothing
        }

        return json.toString();
    }

    /**
     * @return value corresponding to <code>handle</code> parameter in MAP
     *         specification
     */
    public String getHandle() {
        return mHandle;
    }

    /**
     * @return value corresponding to <code>subject</code> parameter in MAP
     *         specification
     */
    public String getSubject() {
        return mSubject;
    }

    /**
     * @return <code>Date</code> object corresponding to <code>datetime</code>
     *         parameter in MAP specification
     */
    public Date getDateTime() {
        return mDateTime;
    }

    /**
     * @return value corresponding to <code>sender_name</code> parameter in MAP
     *         specification
     */
    public String getSenderName() {
        return mSenderName;
    }

    /**
     * @return value corresponding to <code>sender_addressing</code> parameter
     *         in MAP specification
     */
    public String getSenderAddressing() {
        return mSenderAddressing;
    }

    /**
     * @return value corresponding to <code>replyto_addressing</code> parameter
     *         in MAP specification
     */
    public String getReplytoAddressing() {
        return mReplytoAddressing;
    }

    /**
     * @return value corresponding to <code>recipient_name</code> parameter in
     *         MAP specification
     */
    public String getRecipientName() {
        return mRecipientName;
    }

    /**
     * @return value corresponding to <code>recipient_addressing</code>
     *         parameter in MAP specification
     */
    public String getRecipientAddressing() {
        return mRecipientAddressing;
    }

    /**
     * @return {@link Type} object corresponding to <code>type</code> parameter
     *         in MAP specification
     */
    public Type getType() {
        return mType;
    }

    /**
     * @return value corresponding to <code>size</code> parameter in MAP
     *         specification
     */
    public int getSize() {
        return mSize;
    }

    /**
     * @return {@link .ReceptionStatus} object corresponding to
     *         <code>reception_status</code> parameter in MAP specification
     */
    public ReceptionStatus getReceptionStatus() {
        return mReceptionStatus;
    }

    /**
     * @return value corresponding to <code>attachment_size</code> parameter in
     *         MAP specification
     */
    public int getAttachmentSize() {
        return mAttachmentSize;
    }

    /**
     * @return value corresponding to <code>text</code> parameter in MAP
     *         specification
     */
    public boolean isText() {
        return mText;
    }

    /**
     * @return value corresponding to <code>priority</code> parameter in MAP
     *         specification
     */
    public boolean isPriority() {
        return mPriority;
    }

    /**
     * @return value corresponding to <code>read</code> parameter in MAP
     *         specification
     */
    public boolean isRead() {
        return mRead;
    }

    /**
     * @return value corresponding to <code>sent</code> parameter in MAP
     *         specification
     */
    public boolean isSent() {
        return mSent;
    }

    /**
     * @return value corresponding to <code>protected</code> parameter in MAP
     *         specification
     */
    public boolean isProtected() {
        return mProtected;
    }
}
