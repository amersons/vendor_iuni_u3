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


package org.codeaurora.bluetooth.pxpservice;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothLwPwrProximityMonitor;

/**
 * A Device Properties class. It contains information about bluetooth device
 * which PxpMonitor tried to connect.
 */
public class DeviceProperties {

    public BluetoothGatt gatt = null;

    public String deviceAddress = null;

    public BluetoothGattCharacteristic iasAlertLevelCh = null;

    public BluetoothGattCharacteristic llsAlertLevelCh = null;

    public BluetoothGattCharacteristic txPowerLevelCh = null;

    public BluetoothLwPwrProximityMonitor qcRssiProximityMonitor = null;

    public int pathLossAlertLevel = PxpConsts.ALERT_LEVEL_NO;

    public int maxPathLossThreshold = 147;

    public int minPathLossThreshold = 100;

    public int txPowerLevel = 0;

    public boolean isReading = false;

    public boolean isAlerting = false;

    public boolean connectionState = false;

    public boolean AddedToWhitelist = false;

    public boolean hasIasService = false;

    public boolean hasLlsService = false;

    public boolean hasTxpService = false;

    public boolean disconnect = false;

    public boolean startDiscoverServices = false;

    public boolean failedReadAlertLevel = false;

    public boolean failedReadTxPowerLevel = false;

    public void reset() {
        iasAlertLevelCh = null;
        llsAlertLevelCh = null;
        txPowerLevelCh = null;
        // rssiCallback = null;
        pathLossAlertLevel = PxpConsts.ALERT_LEVEL_NO;
        maxPathLossThreshold = 147;
        minPathLossThreshold = 100;
        txPowerLevel = 0;
        isReading = false;
        isAlerting = false;
        connectionState = false;
        AddedToWhitelist = false;
        hasIasService = false;
        hasLlsService = false;
        hasTxpService = false;
        startDiscoverServices = false;
        failedReadAlertLevel = false;
        failedReadTxPowerLevel = false;
    }
}
