import 'package:downloader/app.dart';
import 'package:downloader/profile_manager.dart';

void main(List<String> arguments) async {
  if (arguments.isNotEmpty && arguments[0] == 'profile') {
    await handleProfileCommand(arguments.sublist(1));
  } else {
    await fetchAndDownloadRelease(arguments);
  }
}
