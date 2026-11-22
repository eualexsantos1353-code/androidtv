package app.stremioorigin.tv;

import android.net.Uri;
import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.DataSource;
import androidx.media3.datasource.DataSpec;
import androidx.media3.datasource.DefaultHttpDataSource;
import androidx.media3.datasource.TransferListener;
import java.io.IOException;

/* YouTube's picture and sound files (googlevideo.com) asked for in 10 MB pieces with its own "range" parameter:
   one request for the whole file is throttled (and cut), pieces come at full speed. Any other address passes
   straight through. (The way NuvioTV plays its trailers.) */
@OptIn(markerClass = UnstableApi.class)
public final class YtChunked implements DataSource.Factory {
    private static final long CHUNK = 10L * 1024 * 1024;
    private final String ua;
    public YtChunked(String ua) { this.ua = ua; }

    @Override public DataSource createDataSource() {
        DefaultHttpDataSource up = new DefaultHttpDataSource.Factory().setUserAgent(ua != null && !ua.isEmpty() ? ua : "Mozilla/5.0 (Macintosh; Intel Mac OS X 15_7_3) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15").setConnectTimeoutMs(15000).setReadTimeoutMs(15000).setAllowCrossProtocolRedirects(true).createDataSource();
        return new Source(up);
    }

    private static final class Source implements DataSource {
        private final DefaultHttpDataSource up;
        private boolean yt; private DataSpec orig; private long total = C.LENGTH_UNSET, start, end, got;
        Source(DefaultHttpDataSource up) { this.up = up; }
        @Override public void addTransferListener(TransferListener l) { up.addTransferListener(l); }
        @Override public long open(DataSpec spec) throws IOException {
            String host = spec.uri.getHost();
            yt = host != null && host.contains("googlevideo.com");
            if (!yt) return up.open(spec);
            orig = spec; start = spec.position; total = spec.length;
            return next();
        }
        private long next() throws IOException {
            end = total != C.LENGTH_UNSET ? Math.min(start + CHUNK - 1, start + total - 1) : start + CHUNK - 1;
            Uri u = orig.uri.buildUpon().appendQueryParameter("range", start + "-" + end).build();
            got = 0;
            up.open(orig.buildUpon().setUri(u).setPosition(0).setLength(C.LENGTH_UNSET).build());
            return total != C.LENGTH_UNSET ? total : C.LENGTH_UNSET;
        }
        @Override public int read(byte[] b, int off, int len) throws IOException {
            if (!yt) return up.read(b, off, len);
            int n = up.read(b, off, len);
            if (n == C.RESULT_END_OF_INPUT) {
                long have = got; up.close();
                if (have < end - start + 1) return C.RESULT_END_OF_INPUT; /* a short piece: the file is done */
                start += have;
                if (total != C.LENGTH_UNSET) { total -= have; if (total <= 0) return C.RESULT_END_OF_INPUT; }
                try { next(); return up.read(b, off, len); } catch (IOException e) { return C.RESULT_END_OF_INPUT; }
            }
            got += n; return n;
        }
        @Override public Uri getUri() { return up.getUri(); }
        @Override public void close() throws IOException { up.close(); orig = null; }
    }
}
