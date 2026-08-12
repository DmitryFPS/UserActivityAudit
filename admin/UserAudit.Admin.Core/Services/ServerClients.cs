using System.Net.Http.Json;
using System.Text.Json;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Services;

public sealed class ServerApiClient(HttpClient http, AdminSettings settings)
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public async Task<(IReadOnlyList<HostSummary> Hosts, IReadOnlyList<TimelineEvent> Timeline, IReadOnlyList<AlertSummary> Alerts)>
        FetchDashboardAsync(CancellationToken cancellationToken = default)
    {
        var hostsTask = http.GetFromJsonAsync<List<HostSummary>>(
            Combine(settings.IngestBaseUrl, "/api/v1/ingest/hosts"), JsonOptions, cancellationToken);
        var timelineTask = http.GetFromJsonAsync<List<TimelineEvent>>(
            Combine(settings.PortalBaseUrl, "/api/v1/portal/timeline"), JsonOptions, cancellationToken);
        var alertsTask = http.GetFromJsonAsync<List<AlertSummary>>(
            Combine(settings.AlertsBaseUrl, "/api/v1/alerts"), JsonOptions, cancellationToken);

        await Task.WhenAll(hostsTask, timelineTask, alertsTask);

        return (
            hostsTask.Result ?? [],
            timelineTask.Result ?? [],
            alertsTask.Result ?? []);
    }

    private static string Combine(string baseUrl, string path)
    {
        var trimmed = baseUrl.TrimEnd('/');
        return trimmed + path;
    }
}

public sealed class EscrowClient(HttpClient http, AdminSettings settings)
{
    public async Task<string> UnwrapDekAsync(string hostname, CancellationToken cancellationToken = default)
    {
        using var request = new HttpRequestMessage(HttpMethod.Post,
            $"{settings.EscrowBaseUrl.TrimEnd('/')}/api/v1/escrow/unwrap");
        request.Headers.Add("X-UserAudit-Admin-Key", settings.AdminApiKey);
        request.Content = JsonContent.Create(new { Hostname = hostname, WrappedDekBase64 = string.Empty });

        using var response = await http.SendAsync(request, cancellationToken);
        response.EnsureSuccessStatusCode();
        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        using var doc = await JsonDocument.ParseAsync(stream, cancellationToken: cancellationToken);
        return doc.RootElement.GetProperty("wrappedDekBase64").GetString()
               ?? throw new InvalidOperationException("Escrow response missing wrappedDekBase64.");
    }
}
