using System.IO;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Display;

public static class AuditEventFormatter
{
    public static string Describe(AuditEventModel e)
    {
        var cat = e.Category.ToLowerInvariant();
        var act = e.Action.ToLowerInvariant();

        return cat switch
        {
            "window" when act == "focus" => FormatWindowFocus(e),
            "process" => FormatProcess(e, act),
            "session" => FormatSession(e, act),
            "file" => FormatFile(e, act),
            "usb" => FormatUsb(e, act),
            "network" when act == "snapshot" => FormatNetworkSnapshot(e),
            "network" => FormatNetwork(e, act),
            "tamper" => FormatTamper(e, act),
            "clipboard" => FormatClipboard(e, act),
            "forensic" => FormatForensic(e, act),
            _ => Fallback(e),
        };
    }

    public static bool IsNetworkSnapshot(AuditEventModel e) =>
        e.Category.Equals("network", StringComparison.OrdinalIgnoreCase) &&
        e.Action.Equals("snapshot", StringComparison.OrdinalIgnoreCase);

    public static bool IsActivityEvent(AuditEventModel e) => AuditEventFilter.IsEmployeeMonitoringEvent(e);

    public static bool IsProcessNoise(AuditEventModel e) => AuditEventFilter.IsProcessEvent(e);

    private static string FormatWindowFocus(AuditEventModel e)
    {
        var title = GetData(e, "title");
        var path = GetExeName(e);
        if (!string.IsNullOrEmpty(title) && !string.IsNullOrEmpty(path))
        {
            return $"{title} — {path}";
        }

        return !string.IsNullOrEmpty(title) ? title : path ?? "активное окно";
    }

    private static string FormatProcess(AuditEventModel e, string act)
    {
        var name = GetExeName(e) ?? "процесс";
        return act switch
        {
            "start" => $"Запуск: {name}",
            "stop" => $"Завершение: {name}",
            _ => $"{act}: {name}",
        };
    }

    private static string FormatSession(AuditEventModel e, string act)
    {
        var user = e.User ?? GetData(e, "user") ?? "пользователь";
        return act switch
        {
            "login" => $"Вход: {user}",
            "logout" => $"Выход: {user}",
            "lock" => $"Блокировка: {user}",
            "unlock" => $"Разблокировка: {user}",
            _ => $"{act}: {user}",
        };
    }

    private static string FormatFile(AuditEventModel e, string act)
    {
        var path = GetData(e, "path") ?? GetData(e, "target") ?? "файл";
        return act switch
        {
            "create" => $"Создан: {path}",
            "modify" => $"Изменён: {path}",
            "delete" => $"Удалён: {path}",
            "rename" => $"Переименован: {path}",
            _ => $"{act}: {path}",
        };
    }

    private static string FormatUsb(AuditEventModel e, string act)
    {
        var vid = GetData(e, "vid");
        var pid = GetData(e, "pid");
        var label = GetData(e, "volume_label") ?? GetData(e, "friendly_name");
        var id = !string.IsNullOrEmpty(vid) && !string.IsNullOrEmpty(pid) ? $"{vid}:{pid}" : null;
        var detail = !string.IsNullOrEmpty(label) ? label : id ?? "устройство";
        return act switch
        {
            "insert" => $"USB подключён: {detail}",
            "remove" => $"USB отключён: {detail}",
            _ => $"USB {act}: {detail}",
        };
    }

    private static string FormatNetworkSnapshot(AuditEventModel e)
    {
        var conn = GetData(e, "established") ?? GetData(e, "connections");
        return string.IsNullOrEmpty(conn) ? "Снимок сетевых соединений" : $"Сеть: {conn} соединений";
    }

    private static string FormatNetwork(AuditEventModel e, string act)
    {
        var remote = GetData(e, "remote_addr") ?? GetData(e, "remote");
        var proc = GetExeName(e);
        if (!string.IsNullOrEmpty(remote) && !string.IsNullOrEmpty(proc))
        {
            return $"{proc} → {remote}";
        }

        return !string.IsNullOrEmpty(remote) ? $"{act}: {remote}" : act;
    }

    private static string FormatTamper(AuditEventModel e, string act)
    {
        var detail = GetData(e, "detail") ?? GetData(e, "path") ?? act;
        return $"Tamper: {detail}";
    }

    private static string FormatClipboard(AuditEventModel e, string act)
    {
        var preview = GetData(e, "preview") ?? GetData(e, "text");
        return string.IsNullOrEmpty(preview) ? $"Буфер обмена: {act}" : $"Буфер: {preview}";
    }

    private static string FormatForensic(AuditEventModel e, string act)
    {
        var pack = GetData(e, "pack_id") ?? GetData(e, "pack_path");
        return string.IsNullOrEmpty(pack) ? $"Forensic: {act}" : $"Forensic pack: {pack}";
    }

    private static string? GetExeName(AuditEventModel e)
    {
        if (TryGetData(e, "path", out var path) && !string.IsNullOrEmpty(path))
        {
            return Path.GetFileName(path);
        }

        if (TryGetData(e, "exe", out var exe))
        {
            return exe;
        }

        if (TryGetData(e, "image_name", out var image))
        {
            return image;
        }

        if (TryGetData(e, "name", out var name))
        {
            return name;
        }

        return null;
    }

    private static string? GetData(AuditEventModel e, string key) =>
        TryGetData(e, key, out var value) ? value : null;

    private static bool TryGetData(AuditEventModel e, string key, out string value)
    {
        foreach (var pair in e.Data)
        {
            if (pair.Key.Equals(key, StringComparison.OrdinalIgnoreCase))
            {
                value = pair.Value;
                return true;
            }
        }

        value = string.Empty;
        return false;
    }

    private static string Fallback(AuditEventModel e)
    {
        var first = e.Data.Values.FirstOrDefault();
        if (!string.IsNullOrEmpty(first))
        {
            return $"{e.Category}/{e.Action}: {first}";
        }

        return $"{e.Category}/{e.Action}";
    }
}
