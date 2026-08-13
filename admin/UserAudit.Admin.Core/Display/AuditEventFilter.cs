using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Display;

public sealed record MonitoringSummary(
    int TotalEvents,
    int TimelineVisible,
    int WindowFocus,
    int SessionEvents,
    int UsbEvents,
    int FileEvents,
    int ProcessEvents,
    int NetworkSnapshots,
    int TamperEvents);

public static class AuditEventFilter
{
    public static bool IsProcessEvent(AuditEventModel e) =>
        e.Category.Equals("process", StringComparison.OrdinalIgnoreCase);

    public static bool IsTamperEvent(AuditEventModel e) =>
        e.Category.Equals("tamper", StringComparison.OrdinalIgnoreCase);

    public static bool IsEmployeeMonitoringEvent(AuditEventModel e) =>
        e.Category.Equals("window", StringComparison.OrdinalIgnoreCase) ||
        e.Category.Equals("session", StringComparison.OrdinalIgnoreCase) ||
        e.Category.Equals("usb", StringComparison.OrdinalIgnoreCase) ||
        e.Category.Equals("file", StringComparison.OrdinalIgnoreCase) ||
        e.Category.Equals("clipboard", StringComparison.OrdinalIgnoreCase);

    public static MonitoringSummary BuildSummary(IReadOnlyList<AuditEventModel> events)
    {
        return new MonitoringSummary(
            TotalEvents: events.Count,
            TimelineVisible: 0,
            WindowFocus: Count(events, "window", "focus"),
            SessionEvents: CountCategory(events, "session"),
            UsbEvents: CountCategory(events, "usb"),
            FileEvents: CountCategory(events, "file"),
            ProcessEvents: events.Count(IsProcessEvent),
            NetworkSnapshots: events.Count(AuditEventFormatter.IsNetworkSnapshot),
            TamperEvents: events.Count(IsTamperEvent));
    }

    public const int TimelineDisplayLimit = 5000;

    public static IEnumerable<AuditEventModel> ApplyTimelineFilter(
        IReadOnlyList<AuditEventModel> events,
        bool employeeMonitoringMode,
        bool hideNetworkSnapshots,
        bool hideProcessEvents,
        bool hideTamperEvents)
    {
        IEnumerable<AuditEventModel> query = events;

        if (employeeMonitoringMode)
        {
            query = query.Where(IsEmployeeMonitoringEvent);
        }

        if (hideNetworkSnapshots)
        {
            query = query.Where(x => !AuditEventFormatter.IsNetworkSnapshot(x));
        }

        if (hideProcessEvents)
        {
            query = query.Where(x => !IsProcessEvent(x));
        }

        if (hideTamperEvents)
        {
            query = query.Where(x => !IsTamperEvent(x));
        }

        return query.OrderByDescending(x => x.TimestampUtc);
    }

    public static string FormatTimelineFilterStatus(int visibleRows, int matchedTotal, int totalEvents)
    {
        if (matchedTotal > visibleRows)
        {
            return $"Показано {visibleRows:N0} из {matchedTotal:N0} (всего в журнале {totalEvents:N0}). Сузьте фильтры или экспортируйте отчёт.";
        }

        return $"Показано {visibleRows:N0} из {totalEvents:N0}";
    }

    public static string FormatMonitoringSummary(MonitoringSummary summary, int timelineVisible)
    {
        return $"Окна: {summary.WindowFocus} · Сессии: {summary.SessionEvents} · USB: {summary.UsbEvents} · " +
               $"Файлы: {summary.FileEvents} · В хронологии: {timelineVisible} из {summary.TotalEvents} " +
               $"(process: {summary.ProcessEvents}, сеть: {summary.NetworkSnapshots}, tamper: {summary.TamperEvents})";
    }

    private static int CountCategory(IReadOnlyList<AuditEventModel> events, string category) =>
        events.Count(x => x.Category.Equals(category, StringComparison.OrdinalIgnoreCase));

    private static int Count(IReadOnlyList<AuditEventModel> events, string category, string action) =>
        events.Count(x =>
            x.Category.Equals(category, StringComparison.OrdinalIgnoreCase) &&
            x.Action.Equals(action, StringComparison.OrdinalIgnoreCase));
}
