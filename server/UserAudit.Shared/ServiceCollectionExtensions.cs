using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.DependencyInjection;
using UserAudit.Shared.Data;

namespace UserAudit.Shared;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddUserAuditDatabase(this IServiceCollection services, string connectionString)
    {
        services.AddDbContext<UserAuditDbContext>(options =>
            options.UseNpgsql(connectionString));
        return services;
    }

    public static async Task EnsureUserAuditDatabaseAsync(this IServiceProvider services, CancellationToken cancellationToken = default)
    {
        await using var scope = services.CreateAsyncScope();
        var db = scope.ServiceProvider.GetRequiredService<UserAuditDbContext>();
        await db.Database.EnsureCreatedAsync(cancellationToken);
    }
}
