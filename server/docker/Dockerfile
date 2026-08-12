ARG PROJECT=UserAudit.Ingest
FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
ARG PROJECT
WORKDIR /src
COPY Directory.Build.props ./
COPY UserActivityAudit.Server.slnx ./
COPY UserAudit.Shared/ UserAudit.Shared/
COPY UserAudit.Ingest/ UserAudit.Ingest/
COPY UserAudit.Escrow/ UserAudit.Escrow/
COPY UserAudit.Alerts/ UserAudit.Alerts/
COPY UserAudit.Portal/ UserAudit.Portal/
RUN dotnet restore UserActivityAudit.Server.slnx
RUN dotnet publish ${PROJECT}/${PROJECT}.csproj -c Release -o /app /p:UseAppHost=false

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app .
ENV ASPNETCORE_URLS=http://+:8080
