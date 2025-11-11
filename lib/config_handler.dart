import 'dart:convert';
import 'dart:io';
import 'package:downloader/argument_parser.dart';

class ConfigProfile {
  final String name;
  final String? mirrorUrl;
  final bool forceOverwrite;
  final String repo;
  final String? chooseTag;
  final String? path;
  final bool latest;

  ConfigProfile({
    required this.name,
    this.mirrorUrl,
    this.forceOverwrite = false,
    this.repo = 'nostalgia296/asd',
    this.chooseTag,
    this.path,
    this.latest = false,
  });

  factory ConfigProfile.fromJson(Map<String, dynamic> json) {
    return ConfigProfile(
      name: json['name'] as String,
      mirrorUrl: json['mirrorUrl'] as String?,
      forceOverwrite: json['forceOverwrite'] as bool? ?? false,
      repo: json['repo'] as String? ?? 'nostalgia296/asd',
      chooseTag: json['chooseTag'] as String?,
      path: json['path'] as String?,
      latest: json['latest'] as bool? ?? false,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'name': name,
      'mirrorUrl': mirrorUrl,
      'forceOverwrite': forceOverwrite,
      'repo': repo,
      'chooseTag': chooseTag,
      'path': path,
      'latest': latest,
    };
  }

  DownloadConfig toDownloadConfig() {
    final config = DownloadConfig();
    config.mirrorUrl = mirrorUrl;
    config.forceOverwrite = forceOverwrite;
    config.repo = repo;
    config.chooseTag = chooseTag;
    config.path = path;
    config.latest = latest;
    return config;
  }
}

class ConfigHandler {
  static const String _configFileName = '.asd_config.json';
  static List<ConfigProfile>? _cachedProfiles;
  static String? _cachedConfigPath;

  static String getConfigPath() {
    if (_cachedConfigPath != null) {
      return _cachedConfigPath!;
    }

    final homeDir = Platform.environment['ASD_CONFIG_PATH'];
    if (homeDir != null) {
      final homeDirConfig = File(
        '$homeDir${Platform.pathSeparator}$_configFileName',
      );
      if (homeDirConfig.existsSync()) {
        _cachedConfigPath = homeDirConfig.path;
        return _cachedConfigPath!;
      }
    }

    final currentDirConfig = File('${Directory.current.path}/$_configFileName');
    if (currentDirConfig.existsSync()) {
      _cachedConfigPath = currentDirConfig.path;
      return _cachedConfigPath!;
    }

    if (homeDir != null) {
      _cachedConfigPath = '$homeDir${Platform.pathSeparator}$_configFileName';
    } else {
      _cachedConfigPath = '${Directory.current.path}/$_configFileName';
    }
    return _cachedConfigPath!;
  }

  static List<ConfigProfile> loadConfigProfiles() {
    if (_cachedProfiles != null) {
      return _cachedProfiles!;
    }

    final configPath = getConfigPath();
    final configFile = File(configPath);

    if (!configFile.existsSync()) {
      _cachedProfiles = [];
      return _cachedProfiles!;
    }

    try {
      final jsonString = configFile.readAsStringSync();
      final jsonData = json.decode(jsonString) as List<dynamic>;

      if (jsonData.any((item) => item is! Map<String, dynamic>)) {
        throw FormatException('配置文件中的项目格式不正确');
      }

      final profiles = jsonData
          .map((item) => ConfigProfile.fromJson(item as Map<String, dynamic>))
          .toList();

      final names = profiles.map((p) => p.name).toList();
      if (names.toSet().length != names.length) {
        throw FormatException('配置文件中存在重复的配置名称');
      }

      _cachedProfiles = profiles;
      return profiles;
    } catch (e) {
      print('警告: 配置文件格式错误 - $e');
      _cachedProfiles = [];
      return _cachedProfiles!;
    }
  }

  static ConfigProfile? findProfileByName(String name) {
    final profiles = loadConfigProfiles();
    try {
      return profiles.firstWhere((profile) => profile.name == name);
    } catch (e) {
      return null;
    }
  }
}
