package app.stremioorigin.tv;

import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.audio.AudioProcessor;
import androidx.media3.common.audio.BaseAudioProcessor;
import androidx.media3.common.util.UnstableApi;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/* The sound decoded here (16-bit PCM), shaped on its way to the TV: night mode (a compressor: the quiet lines up, the loud
   bangs down) and a volume boost, both under a soft limiter so nothing clips. Sound passed through as Dolby / DTS to the
   TV or a receiver is not touched (it is never decoded here). (The idea of NuvioTV's NightModeAudioProcessor and GainAudioProcessor.) */
@OptIn(markerClass = UnstableApi.class)
public final class AudioFx extends BaseAudioProcessor {
    static volatile boolean night;
    static volatile float gain = 1f;
    private float env;

    @Override protected AudioFormat onConfigure(AudioFormat in) throws UnhandledAudioFormatException {
        if (in.encoding != C.ENCODING_PCM_16BIT) return AudioFormat.NOT_SET; /* (anything else goes by untouched) */
        return in;
    }
    @Override public void queueInput(ByteBuffer in) {
        int n = in.remaining(); if (n == 0) return;
        ByteBuffer out = replaceOutputBuffer(n);
        boolean nm = night; float g0 = gain;
        if (!nm && g0 == 1f) { out.put(in); out.flip(); return; }
        in.order(ByteOrder.nativeOrder()); out.order(ByteOrder.nativeOrder());
        int ch = Math.max(1, inputAudioFormat.channelCount), sr = Math.max(8000, inputAudioFormat.sampleRate);
        float atk = 1f - (float) Math.exp(-1.0 / (0.005 * sr)), rel = 1f - (float) Math.exp(-1.0 / (0.25 * sr));
        final float th = -28f, ratio = 4f, makeup = 10f;
        float[] fr = new float[ch];
        while (in.remaining() >= 2 * ch) {
            float peak = 0;
            for (int c = 0; c < ch; c++) { fr[c] = in.getShort() / 32768f; float a = Math.abs(fr[c]); if (a > peak) peak = a; }
            float g = g0;
            if (nm) {
                env += (peak > env ? atk : rel) * (peak - env);
                float db = 20f * (float) Math.log10(env + 1e-6f), red = db > th ? (th + (db - th) / ratio) - db : 0f;
                g *= (float) Math.pow(10, (red + makeup) / 20f);
            }
            for (int c = 0; c < ch; c++) {
                float x = fr[c] * g, a = Math.abs(x);
                if (a > 0.9f) { a = 0.9f + 0.1f * (float) Math.tanh((a - 0.9f) / 0.1f); x = x < 0 ? -a : a; } /* soft limiter */
                out.putShort((short) Math.max(-32768, Math.min(32767, Math.round(x * 32767f))));
            }
        }
        while (in.hasRemaining()) out.put(in.get()); /* (a cut frame: as it came) */
        out.flip();
    }
    @Override protected void onFlush() { env = 0; }
    @Override protected void onReset() { env = 0; }
}
