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

import org.codeaurora.bluetooth.utils.ObexAppParameters;

import java.io.IOException;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;

final class BluetoothMasRequestSetMessageStatus extends BluetoothMasRequest {

    public enum StatusIndicator {
        READ, DELETED;
    }

    private static final String TYPE = "x-bt/messageStatus";

    public BluetoothMasRequestSetMessageStatus(String handle, StatusIndicator statusInd,
            boolean statusValue) {

        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);
        mHeaderSet.setHeader(HeaderSet.NAME, handle);

        ObexAppParameters oap = new ObexAppParameters();
        oap.add(OAP_TAGID_STATUS_INDICATOR,
                statusInd == StatusIndicator.READ ? STATUS_INDICATOR_READ
                        : STATUS_INDICATOR_DELETED);
        oap.add(OAP_TAGID_STATUS_VALUE, statusValue ? STATUS_YES : STATUS_NO);
        oap.addToHeaderSet(mHeaderSet);
    }

    @Override
    public void execute(ClientSession session) throws IOException {
        executePut(session, FILLER_BYTE);
    }
}
