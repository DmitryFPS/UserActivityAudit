using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;

namespace UserAudit.Portal.Pages;

public class HostsModel(UserAuditDbContext db) : Microsoft.AspNetCore.Mvc.RazorPages.PageModel
{
    public IList<HostSummaryDto> Hosts { get; private set; } = [];

    public async Task OnGetAsync(CancellationToken cancellationToken)
    {
        Hosts = await db.Hosts
            .OrderBy(h => h.Hostname)
            .Select(h => new HostSummaryDto(
                h.Hostname,
                h.FirstSeenUtc,
                h.LastSeenUtc,
                h.LogBlobs.Count,
                h.Alerts.Count(a => a.AcknowledgedAtUtc == null)))
            .ToListAsync(cancellationToken);
    }
}
