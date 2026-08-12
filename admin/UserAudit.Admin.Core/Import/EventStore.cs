using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Import;

/// <summary>
/// In-memory store for standalone (local-only) log analysis.
/// </summary>
public sealed class EventStore
{
    private readonly List<AuditEventModel> _events = [];
    private readonly object _gate = new();

    public DateTimeOffset? LastImportUtc { get; private set; }
    public string? LastKeySource { get; private set; }

    public event Action? Changed;

    public IReadOnlyList<AuditEventModel> GetEvents()
    {
        lock (_gate)
        {
            return _events.OrderByDescending(x => x.TimestampUtc).ToList();
        }
    }

    public void ReplaceLocalEvents(IReadOnlyList<AuditEventModel> events, string? keySource = null)
    {
        lock (_gate)
        {
            _events.Clear();
            _events.AddRange(events);
        }

        LastImportUtc = DateTimeOffset.UtcNow;
        LastKeySource = keySource;
        Changed?.Invoke();
    }

    public int EventCount
    {
        get
        {
            lock (_gate)
            {
                return _events.Count;
            }
        }
    }
}
