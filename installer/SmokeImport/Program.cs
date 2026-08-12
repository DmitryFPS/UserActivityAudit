using UserAudit.Admin.Core.Import;

if (!OperatingSystem.IsWindows())
{
    Console.Error.WriteLine("SmokeImport: Windows only.");
    return 1;
}

var svc = new LocalLogService(new LogImporter());
var result = svc.ImportAll();
if (!result.Success)
{
    Console.Error.WriteLine($"FAIL: {result.Error}");
    return 1;
}

Console.WriteLine($"OK events={result.Events.Count} key={result.KeySource}");
var cats = result.Events.GroupBy(e => e.Category).OrderByDescending(g => g.Count()).Take(5);
foreach (var g in cats)
{
    Console.WriteLine($"  {g.Key}: {g.Count()}");
}

return result.Events.Count > 0 ? 0 : 2;
