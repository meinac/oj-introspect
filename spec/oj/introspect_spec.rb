RSpec.describe Oj::Introspect do
  describe "#parse" do
    let(:parser) { described_class.new }
    let(:test_json) { File.read("./spec/fixtures/test.json") }

    subject(:parsed_data) { parser.parse(test_json) }

    it "injects the byte offsets of each object" do
      expect(parsed_data[:__oj_introspection]).to eq({ start_byte: 0, end_byte: 147 })
      expect(parsed_data["top-level-array"][0][:__oj_introspection]).to eq({ start_byte: 57, end_byte: 141 })
      expect(parsed_data["top-level-array"][0]["array-object"][:__oj_introspection]).to eq({ start_byte: 81, end_byte: 135 })
    end
  end
end
