using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;

namespace UserAudit.Portal.Pages;

public class IndexModel(UserAuditDbContext db) : Microsoft.AspNetCore.Mvc.RazorPages.PageModel
{
    public int HostCount { get; private set; }
    public int OpenAlertCount { get; private set; }
    public int LogBlobCount { get; private set; }

    public async Task OnGetAsync(CancellationToken cancellationToken)
    {
        HostCount = await db.Hosts.CountAsync(cancellationToken);
        OpenAlertCount = await db.Alerts.CountAsync(x => x.AcknowledgedAtUtc == null, cancellationToken);
        LogBlobCount = await db.LogBlobs.CountAsync(cancellationToken);
    }
}
