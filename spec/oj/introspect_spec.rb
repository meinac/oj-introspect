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

    context "when the `filter` option is provided" do
      let(:parser) { described_class.new(filter: filter) }
      let(:test_json) { File.read("./spec/fixtures/vulnerabilities.json") }

      context "when the filtered property is an array" do
        let(:filter) { "remediations" }

        it "injects the byte offsets only for the filtered objects" do
          expect(parsed_data[:__oj_introspection]).to be_nil
          expect(parsed_data["scan"][:__oj_introspection]).to be_nil
          expect(parsed_data["vulnerabilities"][0][:__oj_introspection]).to be_nil
          expect(parsed_data["remediations"][0][:__oj_introspection]).to eq({ start_byte: 1601, end_byte: 1602 })
          expect(parsed_data["remediations"][1][:__oj_introspection]).to eq({ start_byte: 1613, end_byte: 1827 })
        end
      end

      context "when the filtered property is an object" do
        let(:filter) { "details" }

        it "injects the byte offsets only for the filtered objects" do
          expect(parsed_data[:__oj_introspection]).to be_nil
          expect(parsed_data["scan"][:__oj_introspection]).to be_nil
          expect(parsed_data["vulnerabilities"][0][:__oj_introspection]).to be_nil
          expect(parsed_data["vulnerabilities"][0]["details"][:__oj_introspection]).to eq({ start_byte: 1204, end_byte: 1552 })
          expect(parsed_data["vulnerabilities"][0]["details"]["source"][:__oj_introspection]).to eq({ start_byte: 1232, end_byte: 1364 })
          expect(parsed_data["vulnerabilities"][0]["details"]["another-thing"][:__oj_introspection]).to eq({ start_byte: 1400, end_byte: 1538 })
          expect(parsed_data["remediations"][0][:__oj_introspection]).to be_nil
        end
      end
    end
  end
end
