/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *
 * Not a Contribution.
 */
 /*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.qti.digitalpensdkdemos;

import com.qti.snapdragon.sdk.digitalpen.DigitalPenManager;
import com.qti.snapdragon.sdk.digitalpen.DigitalPenManager.OffScreenMode;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

public class FingerStylusDemo extends Activity {

    /* The digital pen manager object */
    private DigitalPenManager mDigitalPenManager = null;

    private View mDrawingView = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mDrawingView = new DrawingView(this);
        setContentView(mDrawingView);

        mDigitalPenManager = new DigitalPenManager(getApplication());

        // Enables the off-screen mode in duplicate mode. Events made by the pen
        // originating from the off-screen area will act as though they originated
        // from the on-screen area.
        mDigitalPenManager.getSettings()
                .setOffScreenMode(OffScreenMode.DUPLICATE)
                .apply();
    }

    private class DrawingView extends View {

        private Path mPath;

        private Paint mPaint;

        private Bitmap mBitmap;

        private Canvas mCanvas;

        private Paint mBitmapPaint;

        public DrawingView(Context c) {
            super(c);

            mPath = new Path();

            mPaint = new Paint();
            mPaint.setAntiAlias(true);
            mPaint.setDither(true);
            mPaint.setColor(Color.RED);
            mPaint.setStyle(Paint.Style.STROKE);
            mPaint.setStrokeJoin(Paint.Join.ROUND);
            mPaint.setStrokeCap(Paint.Cap.ROUND);
            mPaint.setStrokeWidth(10);

            mBitmapPaint = new Paint(Paint.DITHER_FLAG);
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            mBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mBitmap);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            canvas.drawColor(Color.GRAY);

            canvas.drawBitmap(mBitmap, 0, 0, mBitmapPaint);

            canvas.drawPath(mPath, mPaint);
        }

        private void touch_start(float x, float y, int tooltype) {
            switch (tooltype) {
                // If the touch event was caused by a finger touching the screen,
                // the TOOL_TYPE_FINGER is sent in the MotionEvent. If the touch
                // event originated from the pen, a TOOL_TYPE_STYLUS is sent.
                case MotionEvent.TOOL_TYPE_FINGER:
                    mPaint.setColor(Color.GREEN);
                    break;
                case MotionEvent.TOOL_TYPE_STYLUS:
                    mPaint.setColor(Color.RED);
                    break;
            }
            mPath.reset();
            mPath.moveTo(x, y);
        }

        private void touch_move(float x, float y) {
            mPath.lineTo(x, y);
        }

        private void touch_up(float x, float y) {
            mPath.lineTo(x, y);
            // commit the path to our offscreen
            mCanvas.drawPath(mPath, mPaint);
            // kill this so we don't double draw
            mPath.reset();
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            float x = event.getX();
            float y = event.getY();

            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    touch_start(x, y, event.getToolType(0));
                    invalidate();
                    break;
                case MotionEvent.ACTION_MOVE:
                    touch_move(x, y);
                    invalidate();
                    break;
                case MotionEvent.ACTION_UP:
                    touch_up(x, y);
                    invalidate();
                    break;
            }
            return true;
        }
    }

}
