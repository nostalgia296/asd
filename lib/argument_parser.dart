import 'dart:io';
import 'package:downloader/utils.dart';

class DownloadConfig {
  String? mirrorUrl;
  bool forceOverwrite = false;
  String repo = 'nostalgia296/asd';
  String? chooseTag;
  String? path;
  bool latest = false;
}

class ArgumentParser {
  static const String _mirrorOption = '-m';
  static const String _forceOption = '-f';
  static const String _urlOption = '-u';
  static const String _tagOption = '-t';
  static const String _pathOption = '-c';
  static const String _latestOption = '-l';

  static DownloadConfig parse(List<String> arguments) {
    final config = DownloadConfig();

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
      }
    }

    return config;
  }

  static void _printAndExit(String message) {
    print(message);
    exit(1);
  }
}
