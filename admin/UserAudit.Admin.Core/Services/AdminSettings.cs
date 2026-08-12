namespace UserAudit.Admin.Core.Services;

public sealed class AdminSettings
{
    public string IngestBaseUrl { get; set; } = "http://127.0.0.1:8081";
    public string EscrowBaseUrl { get; set; } = "http://127.0.0.1:8082";
    public string AlertsBaseUrl { get; set; } = "http://127.0.0.1:8083";
    public string PortalBaseUrl { get; set; } = "http://127.0.0.1:8080";
    public string AdminApiKey { get; set; } = "dev-admin-key-change-me";
    public string? DekFilePath { get; set; }
    public string DefaultLogDirectory { get; set; } =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "UserAudit", "logs");
}
