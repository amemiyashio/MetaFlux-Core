{
  config,
  lib,
  ...
}:
let
  cfg = config.services.metaflux;
in
{
  options.services.metaflux = {
    enable = lib.mkEnableOption "the MetaFlux local compute service";

    package = lib.mkOption {
      type = lib.types.package;
      description = "MetaFlux daemon package supplied by the packaging integration.";
    };

    socketPath = lib.mkOption {
      type = lib.types.str;
      default = "/run/metaflux/metafluxd.sock";
      description = "Absolute Unix SOCK_SEQPACKET path exposed to clients.";
    };

    user = lib.mkOption {
      type = lib.types.str;
      default = "metaflux";
      description = "System user running metafluxd.";
    };

    group = lib.mkOption {
      type = lib.types.str;
      default = "metaflux";
      description = "Group allowed to connect to the MetaFlux socket.";
    };

    clientUsers = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      description = "Existing users added to the MetaFlux socket group.";
    };
  };

  config = lib.mkIf cfg.enable {
    assertions = [
      {
        assertion = lib.hasPrefix "/" cfg.socketPath;
        message = "services.metaflux.socketPath must be absolute";
      }
    ];

    users.groups.${cfg.group} = { };
    users.users =
      lib.genAttrs cfg.clientUsers (_: {
        extraGroups = [ cfg.group ];
      })
      // {
        ${cfg.user} = {
          isSystemUser = true;
          group = cfg.group;
          home = "/var/lib/metaflux";
          createHome = true;
          description = "MetaFlux compute service";
        };
      };

    systemd.sockets.metafluxd = {
      description = "MetaFlux local compute socket";
      wantedBy = [ "sockets.target" ];
      socketConfig = {
        ListenSequentialPacket = cfg.socketPath;
        SocketMode = "0660";
        SocketUser = cfg.user;
        SocketGroup = cfg.group;
        DirectoryMode = "0755";
        RemoveOnStop = true;
      };
    };

    systemd.services.metafluxd = {
      description = "MetaFlux local compute service";
      requires = [ "metafluxd.socket" ];
      after = [ "metafluxd.socket" ];
      environment.METAFLUX_SOCKET = cfg.socketPath;
      serviceConfig = {
        Type = "simple";
        ExecStart = "${cfg.package}/bin/metafluxd";
        User = cfg.user;
        Group = cfg.group;
        RuntimeDirectory = "metaflux";
        RuntimeDirectoryMode = "0755";
        CacheDirectory = "metaflux";
        CacheDirectoryMode = "0750";
        StateDirectory = "metaflux";
        StateDirectoryMode = "0750";
        NoNewPrivileges = true;
        PrivateDevices = true;
        PrivateTmp = true;
        ProtectClock = true;
        ProtectControlGroups = true;
        ProtectHome = true;
        ProtectKernelLogs = true;
        ProtectKernelModules = true;
        ProtectKernelTunables = true;
        ProtectSystem = "strict";
        RestrictAddressFamilies = [ "AF_UNIX" ];
        RestrictRealtime = true;
        SystemCallArchitectures = "native";
      };
    };
  };
}
