using System.IO;
using System.Net.Http;
using System.Windows;
using Microsoft.Win32;
using UserAudit.Admin.Core.Crypto;
using UserAudit.Admin.Core.Forensic;
using UserAudit.Admin.Core.Import;
using UserAudit.Admin.Core.Reports;
using UserAudit.Admin.Core.Services;

namespace UserAudit.Dashboard;

public partial class MainWindow : Window
{
    private readonly AdminSettings _settings = new();
    private readonly EventStore _store = new();
    private readonly LogImporter _importer = new();
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(30) };
    private byte[]? _dek;

    public MainWindow()
    {
        InitializeComponent();
        BindSettingsToUi();
        _store.Changed += RefreshUi;
        LogFolderBox.Text = _settings.DefaultLogDirectory;
        ReportDatePicker.SelectedDate = DateTime.Today;
        RefreshUi();
    }

    private void BindSettingsToUi()
    {
        PortalUrlBox.Text = _settings.PortalBaseUrl;
        IngestUrlBox.Text = _settings.IngestBaseUrl;
        EscrowUrlBox.Text = _settings.EscrowBaseUrl;
        AlertsUrlBox.Text = _settings.AlertsBaseUrl;
        AdminKeyBox.Text = _settings.AdminApiKey;
        DekPathBox.Text = _settings.DekFilePath ?? string.Empty;
    }

    private void ApplyUiToSettings()
    {
        _settings.PortalBaseUrl = PortalUrlBox.Text.Trim();
        _settings.IngestBaseUrl = IngestUrlBox.Text.Trim();
        _settings.EscrowBaseUrl = EscrowUrlBox.Text.Trim();
        _settings.AlertsBaseUrl = AlertsUrlBox.Text.Trim();
        _settings.AdminApiKey = AdminKeyBox.Text.Trim();
        _settings.DekFilePath = string.IsNullOrWhiteSpace(DekPathBox.Text) ? null : DekPathBox.Text.Trim();
    }

    private void RefreshUi()
    {
        HostCountText.Text = _store.ServerHosts.Count.ToString();
        AlertCountText.Text = _store.ServerAlerts.Count(x => x.AcknowledgedAtUtc is null).ToString();
        LocalEventCountText.Text = _store.LocalEventCount.ToString();
        TimelineGrid.ItemsSource = _store.GetAllEvents()
            .Select(x => new
            {
                TimestampUtc = x.TimestampUtc.ToString("u"),
                Hostname = x.Host,
                x.Category,
                x.Action,
                x.Severity,
            })
            .ToList();
        HostsGrid.ItemsSource = _store.ServerHosts;
        AlertsGrid.ItemsSource = _store.ServerAlerts;
    }

    private async void RefreshServer_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            ApplyUiToSettings();
            SetStatus("Refreshing from server...");
            var client = new ServerApiClient(_http, _settings);
            var data = await client.FetchDashboardAsync();
            _store.SetServerData(data.Hosts, data.Timeline, data.Alerts);
            SetStatus($"Server: {data.Hosts.Count} hosts, {data.Alerts.Count} alerts");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Server refresh failed", MessageBoxButton.OK, MessageBoxImage.Warning);
            SetStatus("Server refresh failed");
        }
    }

    private void BrowseLogFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Select encrypted log directory" };
        if (dialog.ShowDialog() == true)
        {
            LogFolderBox.Text = dialog.FolderName;
        }
    }

    private void BrowseDek_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Select DEK file (32 raw bytes or base64)",
            Filter = "Key files|*.*",
        };
        if (dialog.ShowDialog() == true)
        {
            DekPathBox.Text = dialog.FileName;
        }
    }

    private async void UnwrapDek_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            ApplyUiToSettings();
            var hostname = EscrowHostBox.Text.Trim();
            if (string.IsNullOrWhiteSpace(hostname))
            {
                MessageBox.Show(this, "Enter hostname for escrow unwrap.", "Escrow", MessageBoxButton.OK,
                    MessageBoxImage.Information);
                return;
            }

            SetStatus("Unwrapping DEK via escrow...");
            var escrow = new EscrowClient(_http, _settings);
            var dekBase64 = await escrow.UnwrapDekAsync(hostname);
            _dek = EncryptedLogReader.DecodeDekBase64(dekBase64);
            SetStatus($"DEK unwrapped for {hostname.ToUpperInvariant()}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Escrow unwrap failed", MessageBoxButton.OK, MessageBoxImage.Error);
            SetStatus("Escrow unwrap failed");
        }
    }

    private void ImportLogs_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            ApplyUiToSettings();
            var folder = LogFolderBox.Text.Trim();
            if (!Directory.Exists(folder))
            {
                MessageBox.Show(this, "Log folder not found.", "Import", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var dek = ResolveDek();
            var imported = _importer.ImportDirectory(folder, dek);
            _store.ReplaceLocalEvents(imported);
            SetStatus($"Imported {imported.Count} events from {folder}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Import failed", MessageBoxButton.OK, MessageBoxImage.Error);
            SetStatus("Import failed");
        }
    }

    private byte[] ResolveDek()
    {
        if (_dek is { Length: 32 })
        {
            return _dek;
        }

        var path = DekPathBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(path))
        {
            _dek = EncryptedLogReader.LoadDekFromFile(path);
            return _dek;
        }

        throw new InvalidOperationException("Provide DEK file or unwrap via Escrow first.");
    }

    private void ExportForensicPack_Click(object sender, RoutedEventArgs e)
    {
        var events = _store.GetAllEvents();
        if (events.Count == 0)
        {
            MessageBox.Show(this, "No events to export. Import logs or refresh from server.", "Forensic pack",
                MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        var dialog = new SaveFileDialog
        {
            Filter = "ZIP archive|*.zip",
            FileName = $"forensic-pack-{DateTime.UtcNow:yyyyMMddHHmmss}.zip",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        try
        {
            ForensicPackExporter.ExportZip(events, dialog.FileName);
            SetStatus($"Forensic pack exported: {dialog.FileName}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Export failed", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void DailyExcel_Click(object sender, RoutedEventArgs e) =>
        ExportDailyReport(excel: true);

    private void DailyPdf_Click(object sender, RoutedEventArgs e) =>
        ExportDailyReport(excel: false);

    private void ExportDailyReport(bool excel)
    {
        var day = ReportDatePicker.SelectedDate.HasValue
            ? DateOnly.FromDateTime(ReportDatePicker.SelectedDate.Value)
            : DateOnly.FromDateTime(DateTime.Today);
        var summary = DailyActivityReportGenerator.Build(_store.GetAllEvents(), day);
        var dialog = new SaveFileDialog
        {
            Filter = excel ? "Excel|*.xlsx" : "PDF|*.pdf",
            FileName = excel ? $"daily-{day:yyyy-MM-dd}.xlsx" : $"daily-{day:yyyy-MM-dd}.pdf",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        if (excel)
        {
            DailyActivityReportGenerator.ExportExcel(summary, dialog.FileName);
        }
        else
        {
            DailyActivityReportGenerator.ExportPdf(summary, dialog.FileName);
        }

        SetStatus($"Daily report saved: {dialog.FileName}");
    }

    private void UsbExcel_Click(object sender, RoutedEventArgs e) =>
        ExportUsbReport(excel: true);

    private void UsbPdf_Click(object sender, RoutedEventArgs e) =>
        ExportUsbReport(excel: false);

    private void ExportUsbReport(bool excel)
    {
        var rows = UsbReportGenerator.Build(_store.GetAllEvents());
        var dialog = new SaveFileDialog
        {
            Filter = excel ? "Excel|*.xlsx" : "PDF|*.pdf",
            FileName = excel ? "usb-report.xlsx" : "usb-report.pdf",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        if (excel)
        {
            UsbReportGenerator.ExportExcel(rows, dialog.FileName);
        }
        else
        {
            UsbReportGenerator.ExportPdf(rows, dialog.FileName);
        }

        SetStatus($"USB report saved: {dialog.FileName}");
    }

    private void Exit_Click(object sender, RoutedEventArgs e) => Close();

    private void SetStatus(string text) => StatusText.Text = text;
}
