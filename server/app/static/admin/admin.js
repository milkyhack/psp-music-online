(() => {
  const $ = (id) => document.getElementById(id);
  const toast = $("toast");
  const preview = $("preview");

  function showToast(msg, isError = false) {
    if (!toast) return;
    toast.hidden = false;
    toast.textContent = msg;
    toast.classList.toggle("error", isError);
  }

  function setActionLoading(id, loading) {
    const el = $(id);
    if (el) el.classList.toggle("loading", loading);
  }

  function stars(n) {
    const v = Number(n) || 0;
    if (v <= 0) return '<span class="hint">—</span>';
    return `<span class="stars">${"★".repeat(Math.min(5, Math.round(v)))}${"☆".repeat(Math.max(0, 5 - Math.round(v)))}</span>`;
  }

  async function api(path, opts = {}) {
    const key = sessionStorage.getItem("psp_api_key") || "";
    let res;
    try {
      res = await fetch(path, {
        headers: {
          Accept: "application/json",
          ...(key ? { "X-Api-Key": key } : {}),
          ...(opts.body ? { "Content-Type": "application/json" } : {}),
        },
        ...opts,
      });
    } catch {
      throw new Error("Сервер не отвечает — запусти: cd server && .venv-mac/bin/python -m app");
    }
    if (res.status === 401) {
      const entered = window.prompt("Нужен X-Api-Key");
      if (entered) {
        sessionStorage.setItem("psp_api_key", entered);
        return api(path, opts);
      }
      throw new Error("Нет API-ключа");
    }
    const text = await res.text();
    let data = null;
    try {
      data = text ? JSON.parse(text) : null;
    } catch {
      data = { detail: text };
    }
    if (!res.ok) {
      const detail = data?.detail || res.statusText;
      throw new Error(typeof detail === "string" ? detail : JSON.stringify(detail));
    }
    return data;
  }

  function capItem(ok, warn, label) {
    const state = ok ? "ok" : warn ? "warn" : "bad";
    return `<li><span class="dot ${state}"></span><span>${label}</span></li>`;
  }

  function fillDl(el, rows) {
    el.innerHTML = rows
      .map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`)
      .join("");
  }

  function renderCaps(d) {
    const c = d.capabilities;
    const ff = c.ffmpeg?.available;
    const libOk = c.library_dir_exists;
    const formats = d.formats || {};
    const flac = formats[".flac"] || 0;
    const srcLabel = { bundled: "встроенный", path: "PATH", local: "локальный" };
    const ffSrc = srcLabel[d.streaming.ffmpeg.source] || d.streaming.ffmpeg.source || "ffmpeg";
    const health = d.stream_health || {};
    const persisted = d.library.persisted || d.library.path;
    const savedOk = d.library.matches_saved !== false;
    const list = [
      capItem(libOk, false, libOk ? "Папка библиотеки существует" : "Папка не найдена — исправь MUSIC_DIR"),
      capItem(savedOk, !savedOk, savedOk
        ? `Путь сохранён: <code class="mono">${escapeHtml(persisted)}</code>`
        : `В памяти другой путь, чем в базе — нажми Сохранить`),
      capItem(c.flac_supported, false, `FLAC в сканере · в БД треков: ${flac}`),
      capItem(c.covers_embedded && c.covers_itunes, false, "Обложки: из файла + загрузка iTunes"),
      capItem(c.stream_mp3_passthrough, false, "Стрим: MP3 отдаётся как есть"),
      capItem(ff, !ff, ff
        ? `Стрим: FLAC/WAV/M4A/… → MP3 ${d.streaming.bitrate} (${escapeHtml(ffSrc)})`
        : "Нет ffmpeg — pip install imageio-ffmpeg"),
      capItem(!!health.ok, !health.ok && health.reason !== "empty_catalog",
        health.ok
          ? `Файлы на диске находятся (${health.tracks} треков)`
          : health.reason === "empty_catalog"
            ? "Каталог пуст — запусти скан"
            : `Проблема стрима: нет файла (${health.missing_in_sample || 0} из выборки)`),
    ];
    $("cap-list").innerHTML = list.join("");
  }

  function renderLibrary(d) {
    $("music-path").textContent = d.library.path;
    const meta = $("music-path-meta");
    if (meta) {
      const exists = d.library.exists;
      const saved = d.library.persisted || d.library.path;
      meta.innerHTML =
        `<span class="badge ${exists ? "ok" : "need"}">${exists ? "папка есть" : "папки нет"}</span>` +
        ` <span class="badge ${d.library.matches_saved !== false ? "ok" : "need"}">сохранено</span>` +
        ` <span class="hint-inline">база: <code class="mono">${escapeHtml(saved)}</code></span>`;
    }
    fillDl($("library-stats"), [
      ["Папка есть", d.library.exists ? "да" : "НЕТ"],
      ["Треков в БД", d.counts.tracks],
      ["Исполнителей", d.counts.artists],
      ["Альбомов", d.counts.albums],
      ["Статус скана", d.scan_status || "idle"],
    ]);
    const formats = d.formats || {};
    const keys = Object.keys(formats);
    $("formats").innerHTML = keys.length
      ? keys
          .map((ext) => {
            const cls = ext === ".flac" ? "flac" : ext === ".mp3" ? "mp3" : "";
            return `<span class="fmt ${cls}">${escapeHtml(ext)} × ${formats[ext]}</span>`;
          })
          .join("")
      : `<span class="hint">В БД пока пусто — запусти скан.</span>`;
  }

  function renderCovers(d) {
    const c = d.covers;
    fillDl($("cover-stats"), [
      ["Альбомов с обложкой", `${c.albums_with_cover} / ${c.albums_total}`],
      ["Без обложки", c.albums_missing_cover],
      ["Файлов обложек", c.cover_files_on_disk],
      ["Размер", c.covers_size_human],
    ]);
    $("cover-hint").textContent =
      `Источники: теги → folder.jpg → iTunes → Deezer → Cover Art Archive. Папка: ${c.covers_dir}. Догрузка: POST /api/covers/backfill`;
  }

  function renderStream(d) {
    const s = d.streaming;
    const ff = s.ffmpeg;
    const health = d.stream_health || {};
    const srcLabel = { bundled: "встроенный", path: "PATH", local: "локальный" };
    fillDl($("stream-stats"), [
      ["ffmpeg", ff.available ? `есть (${srcLabel[ff.source] || ff.source || "?"})` : "НЕТ"],
      ["Отдаём клиенту", `${s.target_format} @ ${s.bitrate}`],
      ["Без конвертации", (s.passthrough || [".mp3"]).join(", ")],
      ["Файлы на диске", health.ok ? "ок" : (health.reason === "empty_catalog" ? "пусто" : "есть дыры")],
      ["Файлов в кэше", s.cache.files],
      ["Размер кэша", s.cache.size_human],
    ]);
    $("stream-hint").textContent = health.ok
      ? `Проверка: трек #${health.sample_id} найден. ${ff.version || "ffmpeg готов"}`
      : health.reason === "empty_catalog"
        ? "Сначала укажи папку и сделай скан."
        : `Не найден файл: ${health.sample_path || "—"}. Проверь MUSIC_DIR.`;
  }

  function renderLan(d, s) {
    const setup = d.psp_setup || s.psp_setup || {};
    const ips = setup.lan_ips || d.lan_ips || s.lan_ips || [];
    const port = setup.port ?? s.port ?? d.listen?.port ?? 8084;
    const line = setup.setup_line || (ips[0] ? `${ips[0]} ${port}` : "");
    const setupEl = $("psp-setup-line");
    const subEl = $("psp-setup-sub");
    const copyBtn = $("btn-copy-psp");
    if (setupEl) {
      setupEl.textContent = line || `<IP_этого_ПК> ${port}`;
    }
    if (subEl) {
      subEl.textContent = line
        ? `server.cfg: ${line} · админка в LAN: ${setup.admin_lan || `http://${ips[0]}:${port}/`}`
        : "LAN IP не определился — проверь Wi‑Fi и перезапусти сервер.";
    }
    if (copyBtn) {
      copyBtn.hidden = !line;
      copyBtn.onclick = async () => {
        try {
          await navigator.clipboard.writeText(line);
          showToast(`Скопировано: ${line}`);
        } catch {
          showToast(line, false);
        }
      };
    }
    const box = $("lan-ips");
    if (box) {
      box.innerHTML = ips.length
        ? ips.map((ip) => `<code class="ip-chip">${escapeHtml(ip)} ${port}</code>`).join("")
        : `<span class="hint">LAN IP не определился — посмотри в настройках роутера.</span>`;
    }
    $("lan-hint-detail").innerHTML =
      `Сервер слушает <code class="mono">${escapeHtml(s.host || "0.0.0.0")}:${escapeHtml(String(port))}</code>. ` +
      `На PSP в Setup IP/Port вводи строку выше — ` +
      `<strong>IP и порт через пробел</strong>.`;
  }

  function renderStatus(d) {
    const pill = $("status-pill");
    if (!pill) return;
    const libOk = !!d.library?.exists;
    const health = d.stream_health || {};
    const ok = libOk && (health.ok || health.reason === "empty_catalog");
    pill.textContent = ok ? "сервер ок" : "проверь путь";
    pill.className = "status-pill " + (ok ? "ok" : "bad");
  }

  function renderSettings(s) {
    $("inp-music-dir").value = s.music_dir || "";
    $("inp-host").value = s.host || "";
    $("inp-port").value = s.port ?? 8084;
    $("inp-api-key").value = s.api_key || "";
    $("settings-meta").innerHTML =
      `Стрим: MP3 как есть, остальное ${escapeHtml(s.stream_bitrate)} · ` +
      `Данные: <code class="mono">${escapeHtml(s.data_dir)}</code>` +
      (s.music_dir_exists ? "" : ` · <span class="badge need">папка не найдена</span>`);
  }

  function renderTracks(rows) {
    const body = $("tracks-body");
    if (!rows.length) {
      body.innerHTML = `<tr><td colspan="6">Каталог пуст. Укажи MUSIC_DIR и сделай скан.</td></tr>`;
      return;
    }
    body.innerHTML = rows
      .map((t) => {
        const thumb = t.cover_url
          ? `<img class="thumb" src="${t.cover_url}" alt="" />`
          : `<div class="thumb" title="нет обложки"></div>`;
        const streamBadge = t.needs_transcode
          ? `<span class="badge need">транскод</span>`
          : `<span class="badge ok">mp3</span>`;
        const coverBadge = t.has_cover
          ? `<span class="badge ok">есть</span>`
          : `<span class="badge">нет</span>`;
        return `<tr>
          <td>${thumb}</td>
          <td>${escapeHtml(t.title)}</td>
          <td>${escapeHtml(t.artist)}</td>
          <td><span class="badge">${escapeHtml(t.format)}</span></td>
          <td>
            ${streamBadge}
            <button type="button" class="btn btn-outline btn-sm btn-play" data-url="${t.stream_url}">▶</button>
          </td>
          <td>${coverBadge}</td>
        </tr>`;
      })
      .join("");

    body.querySelectorAll(".btn-play").forEach((btn) => {
      btn.addEventListener("click", () => {
        preview.src = btn.dataset.url;
        preview.play().catch(() => showToast("Не удалось воспроизвести (проблема с ffmpeg?)", true));
      });
    });
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  let browseTab = "genres";

  function renderBrowseSummary(summary) {
    const el = $("browse-summary");
    if (!el || !summary) return;
    const cards = [
      ["Жанров", summary.genres || 0],
      ["Оценок", summary.rated_tracks || 0],
      ["Из сети", summary.albums_with_network_info || 0],
      ["Bio", summary.artists_with_bio || 0],
    ];
    el.innerHTML = cards
      .map(
        ([label, val]) =>
          `<div class="stat-card"><strong>${val}</strong><span>${escapeHtml(label)}</span></div>`
      )
      .join("");
  }

  async function loadBrowseTab(tab) {
    browseTab = tab;
    document.querySelectorAll(".browse-tab").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.tab === tab);
    });
    const head = $("browse-head");
    const body = $("browse-body");
    const detail = $("browse-detail");
    if (detail) detail.hidden = true;
    try {
      if (tab === "genres") {
        head.innerHTML = "<tr><th>Жанр</th><th>Треков</th><th>Альбомов</th></tr>";
        const rows = await api("/api/genres?limit=50");
        body.innerHTML = rows.length
          ? rows.map((r) => `<tr class="clickable" data-genre="${escapeHtml(r.genre)}">
              <td>${escapeHtml(r.genre)}</td><td>${r.track_count}</td><td>${r.album_count}</td>
            </tr>`).join("")
          : `<tr><td colspan="3">Жанры не найдены — сделай скан.</td></tr>`;
      } else if (tab === "rated") {
        head.innerHTML = "<tr><th>Альбом</th><th>Исполнитель</th><th>Мой рейтинг</th><th>Треков</th></tr>";
        const rows = await api("/api/albums?min_user_rating=1&sort=rating&limit=40");
        body.innerHTML = rows.length
          ? rows.map((r) => `<tr class="clickable" data-album="${r.id}">
              <td>${escapeHtml(r.name)}</td><td>${escapeHtml(r.artist)}</td>
              <td>${stars(r.user_rating)}</td><td>${r.tracks}</td></tr>`).join("")
          : `<tr><td colspan="4">Пока нет оценок — ставь звёзды на PSP.</td></tr>`;
      } else if (tab === "network") {
        head.innerHTML = "<tr><th>Альбом</th><th>Жанр</th><th>Год</th><th>Сеть</th></tr>";
        const rows = await api("/api/albums?sort=external&limit=40");
        body.innerHTML = rows
          .filter((r) => r.external_score || r.summary || r.genre)
          .slice(0, 40)
          .map((r) => `<tr class="clickable" data-album="${r.id}">
              <td>${escapeHtml(r.name)} · ${escapeHtml(r.artist)}</td>
              <td>${escapeHtml(r.genre || "—")}</td><td>${r.year || "—"}</td>
              <td>${r.external_score != null ? r.external_score : "—"}</td></tr>`).join("") ||
          `<tr><td colspan="4">Нет данных из сети — нажми «Инфо из сети».</td></tr>`;
      } else {
        head.innerHTML = "<tr><th>Исполнитель</th><th>Жанр</th><th>Альбомов</th><th>Треков</th></tr>";
        const rows = await api("/api/artists?limit=50");
        body.innerHTML = rows.length
          ? rows.map((r) => `<tr class="clickable" data-artist="${r.id}">
              <td>${escapeHtml(r.name)}</td><td>${escapeHtml(r.genre || "—")}</td>
              <td>${r.albums ?? "—"}</td><td>${r.tracks}</td></tr>`).join("")
          : `<tr><td colspan="4">Каталог пуст.</td></tr>`;
      }
      body.querySelectorAll("tr.clickable").forEach((row) => {
        row.addEventListener("click", () => showBrowseDetail(row));
      });
    } catch (e) {
      body.innerHTML = `<tr><td colspan="4">${escapeHtml(e.message)}</td></tr>`;
    }
  }

  async function showBrowseDetail(row) {
    const detail = $("browse-detail");
    if (!detail) return;
    const albumId = row.dataset.album;
    const artistId = row.dataset.artist;
    const genre = row.dataset.genre;
    detail.hidden = false;
    detail.textContent = "Загрузка…";
    try {
      if (albumId) {
        const a = await api(`/api/albums/${albumId}`);
        detail.innerHTML =
          `<h3>${escapeHtml(a.name)}</h3>` +
          `<div class="meta-row">` +
          `<span class="badge">${escapeHtml(a.genre || "жанр ?")}</span>` +
          `<span class="badge">${a.year || "?"}</span>` +
          `${stars(a.user_rating)}` +
          (a.external_score != null ? `<span class="badge ok">сеть ${a.external_score}</span>` : "") +
          `</div>` +
          `<p class="hint">${escapeHtml(a.artist)} · ${a.tracks || (a.tracks_list || []).length} треков</p>` +
          `<p class="hint">${escapeHtml(a.summary || "Описание пока пустое — нажми «Инфо из сети».")}</p>` +
          `<ul>${(a.tracks_list || []).slice(0, 10).map((t) =>
            `<li>${escapeHtml(t.title)} ${t.rating ? stars(t.rating) : ""}</li>`).join("")}</ul>`;
      } else if (artistId) {
        const a = await api(`/api/artists/${artistId}`);
        detail.innerHTML =
          `<h3>${escapeHtml(a.name)}</h3>` +
          `<div class="meta-row">` +
          (a.country ? `<span class="badge">${escapeHtml(a.country)}</span>` : "") +
          (a.genre ? `<span class="badge">${escapeHtml(a.genre)}</span>` : "") +
          (a.external_score != null ? `<span class="badge ok">сеть ${a.external_score}</span>` : "") +
          `</div>` +
          `<p class="hint">${escapeHtml(a.bio || "Bio пока пустое — нажми «Инфо из сети».")}</p>` +
          `<ul>${(a.albums_list || []).slice(0, 12).map((al) =>
            `<li>${escapeHtml(al.name)} (${al.year || "?"}) ${al.user_rating ? stars(al.user_rating) : ""}</li>`).join("")}</ul>`;
      } else if (genre) {
        const rows = await api(`/api/albums?genre=${encodeURIComponent(genre)}&limit=20`);
        detail.innerHTML =
          `<h3>Жанр: ${escapeHtml(genre)}</h3>` +
          `<ul>${rows.map((al) =>
            `<li>${escapeHtml(al.name)} · ${escapeHtml(al.artist)} (${al.year || "?"})</li>`).join("")}</ul>`;
      }
    } catch (e) {
      detail.textContent = e.message;
    }
  }

  document.querySelectorAll(".browse-tab").forEach((btn) => {
    btn.addEventListener("click", () => loadBrowseTab(btn.dataset.tab));
  });

  async function refreshBrowse() {
    const summary = await api("/api/browse");
    renderBrowseSummary(summary);
    await loadBrowseTab(browseTab);
  }

  async function refresh() {
    const [diag, settings, tracks] = await Promise.all([
      api("/api/admin/diagnostics"),
      api("/api/admin/settings"),
      api("/api/admin/tracks?limit=40"),
    ]);
    renderCaps(diag);
    renderLibrary(diag);
    renderCovers(diag);
    renderStream(diag);
    renderSettings(settings);
    renderLan(diag, settings);
    renderStatus(diag);
    renderTracks(tracks);
    await refreshBrowse();
    await refreshLogs();
  }

  $("btn-refresh").addEventListener("click", () => {
    refresh().catch((e) => showToast(e.message, true));
  });

  $("settings-form").addEventListener("submit", async (ev) => {
    ev.preventDefault();
    try {
      const body = {
        music_dir: $("inp-music-dir").value.trim(),
        host: $("inp-host").value.trim(),
        port: Number($("inp-port").value),
        api_key: $("inp-api-key").value,
      };
      const res = await api("/api/admin/settings", {
        method: "PUT",
        body: JSON.stringify(body),
      });
      const saved = res.saved_music_dir || body.music_dir;
      const exists = res.music_dir_exists ? "папка есть" : "папки нет";
      showToast(
        res.restart_needed
          ? `Сохранено ${saved} (${exists}). Перезапусти сервер для HOST/PORT.`
          : `Путь сохранён: ${saved} (${exists}).`
      );
      await refresh();
    } catch (e) {
      showToast(e.message, true);
    }
  });

  async function doScan(fetchCovers) {
    const btnId = fetchCovers ? "btn-scan" : "btn-scan-nocover";
    setActionLoading(btnId, true);
    try {
      showToast(fetchCovers ? "Скан запущен…" : "Быстрый скан…");
      // Metadata (MusicBrainz/iTunes) is slow and used to look "stuck at 50".
      // Covers stay optional via the two buttons; metadata is a separate concern.
      await api(
        `/api/scan?fetch_covers=${fetchCovers ? "true" : "false"}&fetch_metadata=false`,
        { method: "POST" }
      );
      for (let n = 0; n < 400; n++) {
        const st = await api("/api/scan/status");
        if (st.status !== "running") {
          if (st.error) {
            throw new Error(st.error);
          }
          showToast(
            `Готово: +${st.added || 0}, обновлено ${st.updated || 0}, удалено ${st.removed || 0} · всего ${st.total || st.track_count || 0}`
          );
          break;
        }
        const phase = st.phase ? String(st.phase) : "files";
        const totalHint = st.total ? ` / ${st.total}` : "";
        showToast(`Скан… ${st.progress || 0}${totalHint} · ${phase}`);
        await new Promise((r) => setTimeout(r, 800));
        if (n === 399) {
          throw new Error("Скан всё ещё идёт — смотри статус позже");
        }
      }
      await refresh();
    } catch (e) {
      showToast(e.message, true);
    } finally {
      setActionLoading(btnId, false);
    }
  }

  $("btn-scan").addEventListener("click", () => doScan(true));
  $("btn-scan-nocover").addEventListener("click", () => doScan(false));

  const btnBackfill = $("btn-covers-backfill");
  if (btnBackfill) {
    btnBackfill.addEventListener("click", async () => {
      setActionLoading("btn-covers-backfill", true);
      try {
        showToast("Догружаем обложки…");
        const res = await api("/api/covers/backfill", { method: "POST" });
        showToast(`Обложки: ${res.filled} из ${res.attempted}`);
        await refresh();
      } catch (e) {
        showToast(String(e.message || e), true);
      } finally {
        setActionLoading("btn-covers-backfill", false);
      }
    });
  }

  const btnMeta = $("btn-metadata-backfill");
  if (btnMeta) {
    btnMeta.addEventListener("click", async () => {
      setActionLoading("btn-metadata-backfill", true);
      try {
        showToast("Загружаем инфо из iTunes / MusicBrainz… (может занять минуту)");
        const res = await api("/api/metadata/backfill", { method: "POST" });
        showToast(
          `Сеть: альбомов ${res.albums_filled}/${res.albums_attempted}, артистов ${res.artists_filled}/${res.artists_attempted}`
        );
        await refresh();
      } catch (e) {
        showToast(e.message, true);
      } finally {
        setActionLoading("btn-metadata-backfill", false);
      }
    });
  }

  $("btn-clear-cache").addEventListener("click", async () => {
    try {
      const res = await api("/api/admin/cache/clear", { method: "POST" });
      showToast(`Кэш очищен (${res.removed} файлов).`);
      await refresh();
    } catch (e) {
      showToast(e.message, true);
    }
  });

  async function refreshLogs() {
    const body = $("logs-body");
    const detail = $("log-detail");
    if (!body) return;
    try {
      const rows = await api("/api/client/logs?limit=40");
      if (!rows.length) {
        body.innerHTML = `<tr><td colspan="5">Пока пусто — поиграй на PSP в сети</td></tr>`;
        if (detail) detail.hidden = true;
        return;
      }
      body.innerHTML = rows
        .map((r) => {
          const t = r.received_at
            ? new Date(Number(r.received_at) * 1000).toLocaleString()
            : r.file;
          const last = `${r.last_location || ""} ${r.last_message || ""}`.trim() || "—";
          return `<tr>
            <td class="mono">${t}</td>
            <td class="mono">${r.app_version || "?"} <span class="hint">(${r.app_code || 0})</span></td>
            <td>${r.event_count || 0}</td>
            <td class="mono">${last}</td>
            <td><button type="button" class="btn btn-outline btn-sm btn-log-open" data-file="${r.file}">Открыть</button></td>
          </tr>`;
        })
        .join("");
      body.querySelectorAll(".btn-log-open").forEach((btn) => {
        btn.addEventListener("click", async () => {
          try {
            const file = btn.getAttribute("data-file");
            const data = await api(`/api/client/logs/${encodeURIComponent(file)}`);
            if (detail) {
              detail.hidden = false;
              detail.textContent = JSON.stringify(data, null, 2);
            }
          } catch (e) {
            showToast(e.message, true);
          }
        });
      });
    } catch (e) {
      body.innerHTML = `<tr><td colspan="5">${e.message}</td></tr>`;
    }
  }

  const btnLogs = $("btn-logs-refresh");
  if (btnLogs) {
    btnLogs.addEventListener("click", () => refreshLogs().catch((e) => showToast(e.message, true)));
  }

  refresh().catch((e) => showToast(e.message, true));
})();
