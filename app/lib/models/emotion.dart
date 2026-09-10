class EmotionStyle {
  const EmotionStyle({
    required this.id,
    required this.label,
    required this.bg,
    required this.face,
  });

  final String id;
  final String label;
  final int bg;
  final int face;

  static const values = <EmotionStyle>[
    EmotionStyle(id: 'idle', label: '待机', bg: 0xFF102A44, face: 0xFF5D9CEC),
    EmotionStyle(id: 'listen', label: '聆听', bg: 0xFF0B3D0B, face: 0xFF2ECC71),
    EmotionStyle(id: 'think', label: '思考', bg: 0xFF2A1248, face: 0xFFC39BD3),
    EmotionStyle(id: 'speak', label: '说话', bg: 0xFF4A2A00, face: 0xFFF5B041),
    EmotionStyle(id: 'quiet', label: '小声', bg: 0xFF2C2C2C, face: 0xFFB0B0B0),
    EmotionStyle(id: 'silent', label: '无声', bg: 0xFF111111, face: 0xFF6E6E6E),
    EmotionStyle(id: 'alarm', label: '闹钟', bg: 0xFF4A0000, face: 0xFFE74C3C),
    EmotionStyle(id: 'offline', label: '没网', bg: 0xFF1B1B1B, face: 0xFF7F8C8D),
  ];

  static EmotionStyle of(String id) {
    return values.firstWhere((item) => item.id == id, orElse: () => values.first);
  }
}
