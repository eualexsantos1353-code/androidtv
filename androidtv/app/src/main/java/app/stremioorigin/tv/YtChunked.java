package app.stremioorigin.tv;

import android.net.Uri;
import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.DataSource;
import androidx.media3.datasource.DataSpec;
import androidx.media3.datasource.okhttp.OkHttpDataSource;
import android.util.Log;
import okhttp3.Dns;
import okhttp3.OkHttpClient;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
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

    /* IPv4 only: the address in the file's link (ip=...) is the one that asked for it, and a piece asked for over IPv6 is refused (403). */
    private static final OkHttpClient HTTP = new OkHttpClient.Builder()
        .dns(new Dns() { @Override public List<InetAddress> lookup(String host) throws UnknownHostException {
            List<InetAddress> v4 = new ArrayList<>(); for (InetAddress a : Dns.SYSTEM.lookup(host)) if (a instanceof Inet4Address) v4.add(a);
            if (v4.isEmpty()) throw new UnknownHostException("no IPv4 address for " + host); return v4; } })
        .connectTimeout(15, TimeUnit.SECONDS).readTimeout(15, TimeUnit.SECONDS).followRedirects(true).followSslRedirects(true).build();

    @Override public DataSource createDataSource() {
        OkHttpDataSource up = new OkHttpDataSource.Factory(HTTP).setUserAgent(ua != null && !ua.isEmpty() ? ua : "Mozilla/5.0 (Macintosh; Intel Mac OS X 15_7_3) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15").createDataSource();
        return new Source(up);
    }

    private static final class Source implements DataSource {
        private final DataSource up;
        private boolean yt, plain; private DataSpec orig; private long total = C.LENGTH_UNSET, start, end, got;
        Source(DataSource up) { this.up = up; }
        @Override public void addTransferListener(TransferListener l) { up.addTransferListener(l); }
        @Override public long open(DataSpec spec) throws IOException {
            String host = spec.uri.getHost();
            yt = host != null && host.contains("googlevideo.com");
            if (!yt) return up.open(spec);
            orig = spec; start = spec.position; total = spec.length;
            return next();
        }
        private String good; /* the server that answered the first piece: the next pieces go to it too */
        private long open1(String host) throws IOException {
            end = total != C.LENGTH_UNSET ? Math.min(start + CHUNK - 1, start + total - 1) : start + CHUNK - 1;
            Uri b = orig.uri; if (host != null) b = b.buildUpon().authority(host).build();
            Uri u = b.buildUpon().appendQueryParameter("range", start + "-" + end).build();
            got = 0;
            Log.i("StremioOrigin", "[ytfile] itag " + b.getQueryParameter("itag") + " " + b.getHost() + " from " + start);
            up.open(orig.buildUpon().setUri(u).setPosition(0).setLength(C.LENGTH_UNSET).build());
            return total != C.LENGTH_UNSET ? total : C.LENGTH_UNSET;
        }
        /* The other servers of the same video (the "mn" list), as NuvioTV tries them: some of them refuse (403). */
        private List<String> others() {
            List<String> out = new ArrayList<>();
            String mn = orig.uri.getQueryParameter("mn"), host = orig.uri.getHost(); if (mn == null || host == null) return out;
            String[] sv = mn.split(",");
            for (int i = 0; i < sv.length; i++) {
                String s = sv[i].trim(); if (s.isEmpty()) continue;
                String alt = host.replaceFirst("^rr\\d+---", "rr" + (i + 1) + "---").replaceFirst("sn-[a-z0-9]+-[a-z0-9]+", s);
                if (!alt.equals(host) && !out.contains(alt)) out.add(alt);
            }
            return out;
        }
        private long next() throws IOException {
            try { return open1(good); }
            catch (IOException e) {
                Log.i("StremioOrigin", "[ytfile] refused: " + e.getMessage() + " (sound track " + orig.uri.getQueryParameter("xtags") + ", client " + orig.uri.getQueryParameter("c") + ")");
                if (good != null || start != orig.position) throw e; /* only the first piece looks for another server */
                for (String h : others()) { try { up.close(); long r = open1(h); good = h; return r; } catch (IOException x) { Log.i("StremioOrigin", "[ytfile] " + h + " refused: " + x.getMessage()); } }
                /* the file as one request, no "range" parameter (the sound's small files, and the ones that the pieces' form does not suit) */
                try { up.close(); Log.i("StremioOrigin", "[ytfile] itag " + orig.uri.getQueryParameter("itag") + " as one request"); long r = up.open(orig); plain = true; return r; }
                catch (IOException x) { Log.i("StremioOrigin", "[ytfile] as one request: " + x.getMessage()); }
                throw e;
            }
        }
        @Override public int read(byte[] b, int off, int len) throws IOException {
            if (!yt || plain) return up.read(b, off, len);
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
