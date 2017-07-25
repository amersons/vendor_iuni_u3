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


package org.codeaurora.bluetooth.utils;

import android.util.Log;

import java.text.ParseException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class BmsgTokenizer {

    private final String mStr;

    private final Matcher mMatcher;

    private int mPos = 0;

    private final int mOffset;

    static public class Property {
        public final String name;
        public final String value;

        public Property(String name, String value) {
            if (name == null || value == null) {
                throw new IllegalArgumentException();
            }

            this.name = name;
            this.value = value;

            Log.v("BMSG >> ", toString());
        }

        @Override
        public String toString() {
            return name + ":" + value;
        }

        @Override
        public boolean equals(Object o) {
            return ((o instanceof Property) && ((Property) o).name.equals(name) && ((Property) o).value
                    .equals(value));
        }
    };

    public BmsgTokenizer(String str) {
        this(str, 0);
    }

    public BmsgTokenizer(String str, int offset) {
        mStr = str;
        mOffset = offset;
        mMatcher = Pattern.compile("(([^:]*):(.*))?\r\n").matcher(str);
        mPos = mMatcher.regionStart();
    }

    public Property next(boolean alwaysReturn) throws ParseException {
        boolean found = false;

        do {
            mMatcher.region(mPos, mMatcher.regionEnd());

            if (!mMatcher.lookingAt()) {
                if (alwaysReturn) {
                    return null;
                }

                throw new ParseException("Property or empty line expected", pos());
            }

            mPos = mMatcher.end();

            if (mMatcher.group(1) != null) {
                found = true;
            }
        } while (!found);

        return new Property(mMatcher.group(2), mMatcher.group(3));
    }

    public Property next() throws ParseException {
        return next(false);
    }

    public String remaining() {
        return mStr.substring(mPos);
    }

    public int pos() {
        return mPos + mOffset;
    }
}
