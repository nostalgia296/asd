import 'dart:io';
import 'package:downloader/utils.dart';
import 'package:downloader/config_handler.dart';

class DownloadConfig {
  String? mirrorUrl;
  bool forceOverwrite = false;
  String repo = 'nostalgia296/asd';
  String? chooseTag;
  String? path;
  bool latest = false;
  String? profileName;
  String? action;
}

class ArgumentParser {
  static const String _mirrorOption = '-m';
  static const String _forceOption = '-f';
  static const String _urlOption = '-u';
  static const String _tagOption = '-t';
  static const String _pathOption = '-c';
  static const String _latestOption = '-l';
  static const String _profileOption = '-p';

  static DownloadConfig parse(List<String> arguments) {
    final config = DownloadConfig();
    String? profileName;

    for (int i = 0; i < arguments.length; i++) {
      switch (arguments[i]) {
        case _mirrorOption:
          if (i + 1 < arguments.length) {
            config.mirrorUrl = arguments[i + 1];
            if (!config.mirrorUrl!.endsWith('/')) {
              config.mirrorUrl = '${config.mirrorUrl!}/';
            }
            i++;
          } else {
            _printAndExit('错误: $_mirrorOption 参数需要提供一个URL');
          }
          break;
        case _forceOption:
          config.forceOverwrite = true;
          break;
        case _urlOption:
          if (i + 1 < arguments.length) {
            config.repo = extractRepoFromUrl(arguments[i + 1]);
            i++;
          } else {
            _printAndExit('错误: $_urlOption 参数需要提供一个URL');
          }
          break;
        case _tagOption:
          if (i + 1 < arguments.length) {
            config.chooseTag = arguments[i + 1];
            i++;
          } else {
            _printAndExit('错误: $_tagOption 参数需要提供一个标签名');
          }
          break;
        case _pathOption:
          if (i + 1 < arguments.length) {
            config.path = arguments[i + 1];
            i++;
          } else {
            _printAndExit('错误: $_pathOption 参数需要提供一个路径');
          }
          break;
        case _latestOption:
          config.latest = true;
          break;
        case _profileOption:
          if (i + 1 < arguments.length) {
            profileName = arguments[i + 1];
            config.profileName = profileName;
            i++;
          } else {
            _printAndExit('错误: $_profileOption 参数需要提供一个配置名称');
          }
          break;
      }
    }

    if (profileName != null) {
      ConfigProfile? profile;
      try {
        profile = ConfigHandler.findProfileByName(profileName);
      } on Exception catch (e) {
        _printAndExit(e.toString());
      }

      if (profile == null) {
        _printAndExit('错误: 找不到名为 "$profileName" 的配置，请检查配置文件中的配置名称');
      } else {
        final profileConfig = profile.toDownloadConfig();

        if (config.mirrorUrl == null && profileConfig.mirrorUrl != null) {
          config.mirrorUrl = profileConfig.mirrorUrl;
          if (!config.mirrorUrl!.endsWith('/')) {
            config.mirrorUrl = '${config.mirrorUrl!}/';
          }
        }
        if (!config.forceOverwrite)
          config.forceOverwrite = profileConfig.forceOverwrite;
        if (config.repo == 'nostalgia296/asd') {
          config.repo = extractRepoFromUrl(profileConfig.repo);
        }
        if (config.chooseTag == null)
          config.chooseTag = profileConfig.chooseTag;
        if (config.path == null) config.path = profileConfig.path;
        if (!config.latest) config.latest = profileConfig.latest;
        if (config.action == null) config.action = profileConfig.action;
      }
    }

    return config;
  }

  static void _printAndExit(String message) {
    print(message);
    exit(1);
  }
}
