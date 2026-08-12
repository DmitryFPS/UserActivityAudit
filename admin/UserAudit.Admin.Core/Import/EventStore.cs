using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Import;

public sealed class EventStore
{
    private readonly List<AuditEventModel> _localEvents = [];
    private readonly object _gate = new();

    public IReadOnlyList<HostSummary> ServerHosts { get; private set; } = [];
    public IReadOnlyList<TimelineEvent> ServerTimeline { get; private set; } = [];
    public IReadOnlyList<AlertSummary> ServerAlerts { get; private set; } = [];

    public event Action? Changed;

    public IReadOnlyList<AuditEventModel> GetAllEvents()
    {
        lock (_gate)
        {
            return _localEvents
                .Concat(ServerTimeline.Select(x => new AuditEventModel
                {
                    EventId = x.EventId,
                    TimestampUtc = x.TimestampUtc,
                    Category = x.Category,
                    Action = x.Action,
                    Severity = x.Severity,
                    Host = x.Hostname,
                    Level = 1,
                }))
                .OrderByDescending(x => x.TimestampUtc)
                .ToList();
        }
    }

    public void ReplaceLocalEvents(IEnumerable<AuditEventModel> events)
    {
        lock (_gate)
        {
            _localEvents.Clear();
            _localEvents.AddRange(events);
        }

        Changed?.Invoke();
    }

    public void SetServerData(
        IReadOnlyList<HostSummary> hosts,
        IReadOnlyList<TimelineEvent> timeline,
        IReadOnlyList<AlertSummary> alerts)
    {
        ServerHosts = hosts;
        ServerTimeline = timeline;
        ServerAlerts = alerts;
        Changed?.Invoke();
    }

    public int LocalEventCount
    {
        get
        {
            lock (_gate)
            {
                return _localEvents.Count;
            }
        }
    }
}
