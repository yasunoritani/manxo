// サンプル単体テスト
describe('サンプルテスト', () => {
  test('1 + 1は2である', () => {
    expect(1 + 1).toBe(2);
  });
  
  test('文字列の結合', () => {
    expect('hello' + ' ' + 'world').toBe('hello world');
  });
});
