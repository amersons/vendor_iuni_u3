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

import com.android.vcard.VCardEntry;
import com.android.vcard.VCardEntry.EmailData;
import com.android.vcard.VCardEntry.NameData;
import com.android.vcard.VCardEntry.PhoneData;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.List;

/**
 * Entry representation of folder listing
 */
public class BluetoothPbapCard {

    public final String handle;

    public final String N;
    public final String lastName;
    public final String firstName;
    public final String middleName;
    public final String prefix;
    public final String suffix;

    public BluetoothPbapCard(String handle, String name) {
        this.handle = handle;

        N = name;

        /*
         * format is as for vCard N field, so we have up to 5 tokens: LastName;
         * FirstName; MiddleName; Prefix; Suffix
         */
        String[] parsedName = name.split(";", 5);

        lastName = parsedName.length < 1 ? null : parsedName[0];
        firstName = parsedName.length < 2 ? null : parsedName[1];
        middleName = parsedName.length < 3 ? null : parsedName[2];
        prefix = parsedName.length < 4 ? null : parsedName[3];
        suffix = parsedName.length < 5 ? null : parsedName[4];
    }

    @Override
    public String toString() {
        JSONObject json = new JSONObject();

        try {
            json.put("handle", handle);
            json.put("N", N);
            json.put("lastName", lastName);
            json.put("firstName", firstName);
            json.put("middleName", middleName);
            json.put("prefix", prefix);
            json.put("suffix", suffix);
        } catch (JSONException e) {
            // do nothing
        }

        return json.toString();
    }

    static public String jsonifyVcardEntry(VCardEntry vcard) {
        JSONObject json = new JSONObject();

        try {
            NameData name = vcard.getNameData();
            json.put("formatted", name.getFormatted());
            json.put("family", name.getFamily());
            json.put("given", name.getGiven());
            json.put("middle", name.getMiddle());
            json.put("prefix", name.getPrefix());
            json.put("suffix", name.getSuffix());
        } catch (JSONException e) {
            // do nothing
        }

        try {
            JSONArray jsonPhones = new JSONArray();

            List<PhoneData> phones = vcard.getPhoneList();

            if (phones != null) {
                for (PhoneData phone : phones) {
                    JSONObject jsonPhone = new JSONObject();
                    jsonPhone.put("type", phone.getType());
                    jsonPhone.put("number", phone.getNumber());
                    jsonPhone.put("label", phone.getLabel());
                    jsonPhone.put("is_primary", phone.isPrimary());

                    jsonPhones.put(jsonPhone);
                }

                json.put("phones", jsonPhones);
            }
        } catch (JSONException e) {
            // do nothing
        }

        try {
            JSONArray jsonEmails = new JSONArray();

            List<EmailData> emails = vcard.getEmailList();

            if (emails != null) {
                for (EmailData email : emails) {
                    JSONObject jsonEmail = new JSONObject();
                    jsonEmail.put("type", email.getType());
                    jsonEmail.put("address", email.getAddress());
                    jsonEmail.put("label", email.getLabel());
                    jsonEmail.put("is_primary", email.isPrimary());

                    jsonEmails.put(jsonEmail);
                }

                json.put("emails", jsonEmails);
            }
        } catch (JSONException e) {
            // do nothing
        }

        return json.toString();
    }
}
