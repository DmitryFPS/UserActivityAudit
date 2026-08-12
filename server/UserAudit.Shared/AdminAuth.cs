namespace UserAudit.Shared;

using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;

public static class AdminAuth
{
    public const string HeaderName = "X-UserAudit-Admin-Key";

    public static bool IsAuthorized(IHeaderDictionary headers, IConfiguration configuration)
    {
        var expected = configuration["UserAudit:Admin:ApiKey"]
            ?? configuration["UserAudit:AdminApiKey"];
        if (string.IsNullOrWhiteSpace(expected))
        {
            return false;
        }

        return headers.TryGetValue(HeaderName, out var provided) && provided.ToString() == expected;
    }
}
