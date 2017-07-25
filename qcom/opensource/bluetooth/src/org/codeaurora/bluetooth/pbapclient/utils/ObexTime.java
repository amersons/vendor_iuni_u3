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

import java.util.Calendar;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class ObexTime {

    private Date mDate;

    public ObexTime(String time) {
        /*
         * match OBEX time string: YYYYMMDDTHHMMSS with optional UTF offset
         * +/-hhmm
         */
        Pattern p = Pattern
                .compile("(\\d{4})(\\d{2})(\\d{2})T(\\d{2})(\\d{2})(\\d{2})(([+-])(\\d{2})(\\d{2}))?");
        Matcher m = p.matcher(time);

        if (m.matches()) {

            /*
             * matched groups are numberes as follows: YYYY MM DD T HH MM SS +
             * hh mm ^^^^ ^^ ^^ ^^ ^^ ^^ ^ ^^ ^^ 1 2 3 4 5 6 8 9 10 all groups
             * are guaranteed to be numeric so conversion will always succeed
             * (except group 8 which is either + or -)
             */

            Calendar cal = Calendar.getInstance();
            cal.set(Integer.parseInt(m.group(1)), Integer.parseInt(m.group(2)) - 1,
                    Integer.parseInt(m.group(3)), Integer.parseInt(m.group(4)),
                    Integer.parseInt(m.group(5)), Integer.parseInt(m.group(6)));

            /*
             * if 7th group is matched then we have UTC offset information
             * included
             */
            if (m.group(7) != null) {
                int ohh = Integer.parseInt(m.group(9));
                int omm = Integer.parseInt(m.group(10));

                /* time zone offset is specified in miliseconds */
                int offset = (ohh * 60 + omm) * 60 * 1000;

                if (m.group(8).equals("-")) {
                    offset = -offset;
                }

                TimeZone tz = TimeZone.getTimeZone("UTC");
                tz.setRawOffset(offset);

                cal.setTimeZone(tz);
            }

            mDate = cal.getTime();
        }
    }

    public ObexTime(Date date) {
        mDate = date;
    }

    public Date getTime() {
        return mDate;
    }

    @Override
    public String toString() {
        if (mDate == null) {
            return null;
        }

        Calendar cal = Calendar.getInstance();
        cal.setTime(mDate);

        /* note that months are numbered stating from 0 */
        return String.format(Locale.US, "%04d%02d%02dT%02d%02d%02d",
                cal.get(Calendar.YEAR), cal.get(Calendar.MONTH) + 1,
                cal.get(Calendar.DATE), cal.get(Calendar.HOUR_OF_DAY),
                cal.get(Calendar.MINUTE), cal.get(Calendar.SECOND));
    }
}
