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
  static DownloadConfig parse(List<String> arguments) {
    final config = DownloadConfig();

    for (int i = 0; i < arguments.length; i++) {
      if (arguments[i] == '-m') {
        if (i + 1 < arguments.length) {
          config.mirrorUrl = arguments[i + 1];
          if (!config.mirrorUrl!.endsWith('/')) {
            config.mirrorUrl = config.mirrorUrl! + '/';
          }
          i++;
        } else {
          print('错误: -m 参数需要提供一个URL');
          exit(1);
        }
      } else if (arguments[i] == '-f') {
        config.forceOverwrite = true;
      } else if (arguments[i] == '-u') {
        if (i + 1 < arguments.length) {
          config.repo = extractRepoFromUrl(arguments[i + 1]);
          i++;
        } else {
          print('错误: -u 参数需要提供一个URL');
          exit(1);
        }
      } else if (arguments[i] == '-t') {
        if (i + 1 < arguments.length) {
          config.chooseTag = arguments[i + 1];
          i++;
        } else {
          print('错误: -t 参数需要提供一个标签名');
          exit(1);
        }
      } else if (arguments[i] == '-c') {
        if (i + 1 < arguments.length) {
          config.path = arguments[i + 1];
          i++;
        } else {
          print('错误: -c 参数需要提供一个路径');
          exit(1);
        }
      } else if (arguments[i] == '-l') {
        config.latest = true;
      }
    }

    return config;
  }
}
