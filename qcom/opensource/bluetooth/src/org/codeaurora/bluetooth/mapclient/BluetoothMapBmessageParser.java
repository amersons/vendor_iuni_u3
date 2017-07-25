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

import com.android.vcard.VCardEntry;
import com.android.vcard.VCardEntryConstructor;
import com.android.vcard.VCardEntryHandler;
import com.android.vcard.VCardParser;
import com.android.vcard.VCardParser_V21;
import com.android.vcard.VCardParser_V30;
import com.android.vcard.exception.VCardException;
import com.android.vcard.exception.VCardVersionException;
import org.codeaurora.bluetooth.mapclient.BluetoothMapBmessage.Status;
import org.codeaurora.bluetooth.mapclient.BluetoothMapBmessage.Type;
import org.codeaurora.bluetooth.utils.BmsgTokenizer;
import org.codeaurora.bluetooth.utils.BmsgTokenizer.Property;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.text.ParseException;

class BluetoothMapBmessageParser {

    private final static String TAG = "BluetoothMapBmessageParser";

    private final static String CRLF = "\r\n";

    private final static Property BEGIN_BMSG = new Property("BEGIN", "BMSG");
    private final static Property END_BMSG = new Property("END", "BMSG");

    private final static Property BEGIN_VCARD = new Property("BEGIN", "VCARD");
    private final static Property END_VCARD = new Property("END", "VCARD");

    private final static Property BEGIN_BENV = new Property("BEGIN", "BENV");
    private final static Property END_BENV = new Property("END", "BENV");

    private final static Property BEGIN_BBODY = new Property("BEGIN", "BBODY");
    private final static Property END_BBODY = new Property("END", "BBODY");

    private final static Property BEGIN_MSG = new Property("BEGIN", "MSG");
    private final static Property END_MSG = new Property("END", "MSG");

    private final static int CRLF_LEN = 2;

    /*
     * length of "container" for 'message' in bmessage-body-content:
     * BEGIN:MSG<CRLF> + <CRLF> + END:MSG<CRFL>
     */
    private final static int MSG_CONTAINER_LEN = 22;

    private BmsgTokenizer mParser;

    private final BluetoothMapBmessage mBmsg;

    private BluetoothMapBmessageParser() {
        mBmsg = new BluetoothMapBmessage();
    }

    static public BluetoothMapBmessage createBmessage(String str) {
        BluetoothMapBmessageParser p = new BluetoothMapBmessageParser();

        try {
            p.parse(str);
        } catch (IOException e) {
            Log.e(TAG, "I/O exception when parsing bMessage", e);
            return null;
        } catch (ParseException e) {
            Log.e(TAG, "Cannot parse bMessage", e);
            return null;
        }

        return p.mBmsg;
    }

    private ParseException expected(Property... props) {
        boolean first = true;
        StringBuilder sb = new StringBuilder();

        for (Property prop : props) {
            if (!first) {
                sb.append(" or ");
            }
            sb.append(prop);
            first = false;
        }

        return new ParseException("Expected: " + sb.toString(), mParser.pos());
    }

    private void parse(String str) throws IOException, ParseException {

        Property prop;

        /*
         * <bmessage-object>::= { "BEGIN:BMSG" <CRLF> <bmessage-property>
         * [<bmessage-originator>]* <bmessage-envelope> "END:BMSG" <CRLF> }
         */

        mParser = new BmsgTokenizer(str + CRLF);

        prop = mParser.next();
        if (!prop.equals(BEGIN_BMSG)) {
            throw expected(BEGIN_BMSG);
        }

        prop = parseProperties();

        while (prop.equals(BEGIN_VCARD)) {

            /* <bmessage-originator>::= <vcard> <CRLF> */

            StringBuilder vcard = new StringBuilder();
            prop = extractVcard(vcard);

            VCardEntry entry = parseVcard(vcard.toString());
            mBmsg.mOriginators.add(entry);
        }

        if (!prop.equals(BEGIN_BENV)) {
            throw expected(BEGIN_BENV);
        }

        prop = parseEnvelope(1);

        if (!prop.equals(END_BMSG)) {
            throw expected(END_BENV);
        }

        /*
         * there should be no meaningful data left in stream here so we just
         * ignore whatever is left
         */

        mParser = null;
    }

    private Property parseProperties() throws ParseException {

        Property prop;

        /*
         * <bmessage-property>::=<bmessage-version-property>
         * <bmessage-readstatus-property> <bmessage-type-property>
         * <bmessage-folder-property> <bmessage-version-property>::="VERSION:"
         * <common-digit>*"."<common-digit>* <CRLF>
         * <bmessage-readstatus-property>::="STATUS:" 'readstatus' <CRLF>
         * <bmessage-type-property>::="TYPE:" 'type' <CRLF>
         * <bmessage-folder-property>::="FOLDER:" 'foldername' <CRLF>
         */

        do {
            prop = mParser.next();

            if (prop.name.equals("VERSION")) {
                mBmsg.mBmsgVersion = prop.value;

            } else if (prop.name.equals("STATUS")) {
                for (Status s : Status.values()) {
                    if (prop.value.equals(s.toString())) {
                        mBmsg.mBmsgStatus = s;
                        break;
                    }
                }

            } else if (prop.name.equals("TYPE")) {
                for (Type t : Type.values()) {
                    if (prop.value.equals(t.toString())) {
                        mBmsg.mBmsgType = t;
                        break;
                    }
                }

            } else if (prop.name.equals("FOLDER")) {
                mBmsg.mBmsgFolder = prop.value;

            }

        } while (!prop.equals(BEGIN_VCARD) && !prop.equals(BEGIN_BENV));

        return prop;
    }

    private Property parseEnvelope(int level) throws IOException, ParseException {

        Property prop;

        /*
         * we can support as many nesting level as we want, but MAP spec clearly
         * defines that there should be no more than 3 levels. so we verify it
         * here.
         */

        if (level > 3) {
            throw new ParseException("bEnvelope is nested more than 3 times", mParser.pos());
        }

        /*
         * <bmessage-envelope> ::= { "BEGIN:BENV" <CRLF> [<bmessage-recipient>]*
         * <bmessage-envelope> | <bmessage-content> "END:BENV" <CRLF> }
         */

        prop = mParser.next();

        while (prop.equals(BEGIN_VCARD)) {

            /* <bmessage-originator>::= <vcard> <CRLF> */

            StringBuilder vcard = new StringBuilder();
            prop = extractVcard(vcard);

            if (level == 1) {
                VCardEntry entry = parseVcard(vcard.toString());
                mBmsg.mRecipients.add(entry);
            }
        }

        if (prop.equals(BEGIN_BENV)) {
            prop = parseEnvelope(level + 1);

        } else if (prop.equals(BEGIN_BBODY)) {
            prop = parseBody();

        } else {
            throw expected(BEGIN_BENV, BEGIN_BBODY);
        }

        if (!prop.equals(END_BENV)) {
            throw expected(END_BENV);
        }

        return mParser.next();
    }

    private Property parseBody() throws IOException, ParseException {

        Property prop;

        /*
         * <bmessage-content>::= { "BEGIN:BBODY"<CRLF> [<bmessage-body-part-ID>
         * <CRLF>] <bmessage-body-property> <bmessage-body-content>* <CRLF>
         * "END:BBODY"<CRLF> } <bmessage-body-part-ID>::="PARTID:" 'Part-ID'
         * <bmessage-body-property>::=[<bmessage-body-encoding-property>]
         * [<bmessage-body-charset-property>]
         * [<bmessage-body-language-property>]
         * <bmessage-body-content-length-property>
         * <bmessage-body-encoding-property>::="ENCODING:"'encoding' <CRLF>
         * <bmessage-body-charset-property>::="CHARSET:"'charset' <CRLF>
         * <bmessage-body-language-property>::="LANGUAGE:"'language' <CRLF>
         * <bmessage-body-content-length-property>::= "LENGTH:" <common-digit>*
         * <CRLF>
         */

        do {
            prop = mParser.next();

            if (prop.name.equals("PARTID")) {
            } else if (prop.name.equals("ENCODING")) {
                mBmsg.mBbodyEncoding = prop.value;

            } else if (prop.name.equals("CHARSET")) {
                mBmsg.mBbodyCharset = prop.value;

            } else if (prop.name.equals("LANGUAGE")) {
                mBmsg.mBbodyLanguage = prop.value;

            } else if (prop.name.equals("LENGTH")) {
                try {
                    mBmsg.mBbodyLength = Integer.valueOf(prop.value);
                } catch (NumberFormatException e) {
                    throw new ParseException("Invalid LENGTH value", mParser.pos());
                }

            }

        } while (!prop.equals(BEGIN_MSG));

        /*
         * <bmessage-body-content>::={ "BEGIN:MSG"<CRLF> 'message'<CRLF>
         * "END:MSG"<CRLF> }
         */

        int messageLen = mBmsg.mBbodyLength - MSG_CONTAINER_LEN;
        int offset = messageLen + CRLF_LEN;
        int restartPos = mParser.pos() + offset;

        /*
         * length is specified in bytes so we need to convert from unicode
         * string back to bytes array
         */

        String remng = mParser.remaining();
        byte[] data = remng.getBytes();

        /* restart parsing from after 'message'<CRLF> */
        mParser = new BmsgTokenizer(new String(data, offset, data.length - offset), restartPos);

        prop = mParser.next(true);

        if (prop != null && prop.equals(END_MSG)) {
            mBmsg.mMessage = new String(data, 0, messageLen);
        } else {

            data = null;

            /*
             * now we check if bMessage can be parsed if LENGTH is handled as
             * number of characters instead of number of bytes
             */

            Log.w(TAG, "byte LENGTH seems to be invalid, trying with char length");

            mParser = new BmsgTokenizer(remng.substring(offset));

            prop = mParser.next();

            if (!prop.equals(END_MSG)) {
                throw expected(END_MSG);
            }

            mBmsg.mMessage = remng.substring(0, messageLen);
        }

        prop = mParser.next();

        if (!prop.equals(END_BBODY)) {
            throw expected(END_BBODY);
        }

        return mParser.next();
    }

    private Property extractVcard(StringBuilder out) throws IOException, ParseException {
        Property prop;

        out.append(BEGIN_VCARD).append(CRLF);

        do {
            prop = mParser.next();
            out.append(prop).append(CRLF);
        } while (!prop.equals(END_VCARD));

        return mParser.next();
    }

    private class VcardHandler implements VCardEntryHandler {

        VCardEntry vcard;

        @Override
        public void onStart() {
        }

        @Override
        public void onEntryCreated(VCardEntry entry) {
            vcard = entry;
        }

        @Override
        public void onEnd() {
        }
    };

    private VCardEntry parseVcard(String str) throws IOException, ParseException {
        VCardEntry vcard = null;

        try {
            VCardParser p = new VCardParser_V21();
            VCardEntryConstructor c = new VCardEntryConstructor();
            VcardHandler handler = new VcardHandler();
            c.addEntryHandler(handler);
            p.addInterpreter(c);
            p.parse(new ByteArrayInputStream(str.getBytes()));

            vcard = handler.vcard;

        } catch (VCardVersionException e1) {

            try {
                VCardParser p = new VCardParser_V30();
                VCardEntryConstructor c = new VCardEntryConstructor();
                VcardHandler handler = new VcardHandler();
                c.addEntryHandler(handler);
                p.addInterpreter(c);
                p.parse(new ByteArrayInputStream(str.getBytes()));

                vcard = handler.vcard;

            } catch (VCardVersionException e2) {
                // will throw below
            } catch (VCardException e2) {
                // will throw below
            }

        } catch (VCardException e1) {
            // will throw below
        }

        if (vcard == null) {
            throw new ParseException("Cannot parse vCard object (neither 2.1 nor 3.0?)",
                    mParser.pos());
        }

        return vcard;
    }
}
