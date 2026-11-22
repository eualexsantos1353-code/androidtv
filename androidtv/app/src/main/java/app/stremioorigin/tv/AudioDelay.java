package app.stremioorigin.tv;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.StreamKey;
import androidx.media3.common.Timeline;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.LoadingInfo;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.source.WrappingMediaSource;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;
import java.io.IOException;
import java.util.List;

/* The sound later or sooner than the picture (the viewer's Audio delay). The sound is the clock the picture follows, so the
   picture's own times move the other way: it works the same for sound decoded here and for Dolby / DTS passed through to the
   TV. (NuvioTV's AudioDelayMediaSource, in Java.) */
@OptIn(markerClass = UnstableApi.class)
final class AudioDelay {
    static volatile long us; /* + the sound later */

    static final class Source extends WrappingMediaSource {
        Source(MediaSource m) { super(m); }
        @Override public MediaPeriod createPeriod(MediaSource.MediaPeriodId id, Allocator al, long startUs) { return new Period(mediaSource.createPeriod(id, al, startUs)); }
        @Override public void releasePeriod(MediaPeriod p) { mediaSource.releasePeriod(((Period) p).inner); }
        @Override protected void onChildSourceInfoRefreshed(Timeline t) { refreshSourceInfo(t); }
    }

    private static final class Period implements MediaPeriod, MediaPeriod.Callback {
        final MediaPeriod inner; private Callback cb;
        Period(MediaPeriod p) { inner = p; }
        @Override public void prepare(Callback c, long posUs) { cb = c; inner.prepare(this, posUs); }
        @Override public void maybeThrowPrepareError() throws IOException { inner.maybeThrowPrepareError(); }
        @Override public TrackGroupArray getTrackGroups() { return inner.getTrackGroups(); }
        @Override public List<StreamKey> getStreamKeys(List<ExoTrackSelection> s) { return inner.getStreamKeys(s); }
        @Override public long selectTracks(@Nullable ExoTrackSelection[] sel, boolean[] keep, @Nullable SampleStream[] streams, boolean[] reset, long posUs) {
            SampleStream[] child = new SampleStream[streams.length];
            for (int i = 0; i < streams.length; i++) child[i] = streams[i] instanceof Shifted ? ((Shifted) streams[i]).child : streams[i];
            long r = inner.selectTracks(sel, keep, child, reset, posUs);
            for (int i = 0; i < streams.length; i++) {
                SampleStream c = child[i];
                boolean video = c != null && sel[i] != null && MimeTypes.getTrackType(sel[i].getSelectedFormat().sampleMimeType) == C.TRACK_TYPE_VIDEO;
                if (c == null) streams[i] = null;
                else if (video) streams[i] = streams[i] instanceof Shifted && ((Shifted) streams[i]).child == c ? streams[i] : new Shifted(c);
                else streams[i] = c;
            }
            return r;
        }
        @Override public void discardBuffer(long posUs, boolean toKey) { inner.discardBuffer(posUs, toKey); }
        @Override public long readDiscontinuity() { return inner.readDiscontinuity(); }
        @Override public long seekToUs(long posUs) { return inner.seekToUs(posUs); }
        @Override public long getAdjustedSeekPositionUs(long posUs, SeekParameters sp) { return inner.getAdjustedSeekPositionUs(posUs, sp); }
        @Override public long getBufferedPositionUs() { return inner.getBufferedPositionUs(); }
        @Override public long getNextLoadPositionUs() { return inner.getNextLoadPositionUs(); }
        @Override public boolean continueLoading(LoadingInfo li) { return inner.continueLoading(li); }
        @Override public boolean isLoading() { return inner.isLoading(); }
        @Override public void reevaluateBuffer(long posUs) { inner.reevaluateBuffer(posUs); }
        @Override public void onPrepared(MediaPeriod p) { if (cb != null) cb.onPrepared(this); }
        @Override public void onContinueLoadingRequested(MediaPeriod p) { if (cb != null) cb.onContinueLoadingRequested(this); }
    }

    private static final class Shifted implements SampleStream {
        final SampleStream child;
        Shifted(SampleStream c) { child = c; }
        @Override public boolean isReady() { return child.isReady(); }
        @Override public void maybeThrowError() throws IOException { child.maybeThrowError(); }
        @Override public int readData(FormatHolder fh, DecoderInputBuffer b, int flags) {
            int r = child.readData(fh, b, flags);
            if (r == C.RESULT_BUFFER_READ && !b.isEndOfStream()) b.timeUs -= us;
            return r;
        }
        @Override public int skipData(long posUs) { return child.skipData(Math.max(0L, posUs + us)); }
    }
}
