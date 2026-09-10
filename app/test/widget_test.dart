import 'package:flutter_test/flutter_test.dart';

import 'package:bondwatch/main.dart';

void main() {
  testWidgets('app loads fake watch tab', (tester) async {
    await tester.pumpWidget(const BondWatchApp());
    await tester.pump();
    expect(find.textContaining('BONDWATCH'), findsOneWidget);
  });
}
